//
//  ofxUserContentUpload.cpp
//  CollectionTable
//
//  Created by Oriol Ferrer Mesià on 01/02/15.
//
//

#include "ofxUserContentUpload.h"
#include "ofxXmlSettings.h"
#include "ofxRemoteUIServer.h"

ofxUserContentUpload::FailedJobPolicy ofxUserContentUpload::defaultPolicy = FailedJobPolicy();


ofxUserContentUpload::~ofxUserContentUpload(){
	ofLogNotice("ofxUserContentUpload") << "~ofxUserContentUpload()";
	if(isThreadRunning()){
		waitForThread(true);
	}
}

ofxUserContentUpload::ofxUserContentUpload(){

	//given a serverside http status code after executing a job
	//should the job be deleted (true) or stored for a Retry Later (false)?
	//if no specification, job will BE RETRIED LATER

	//default policy is FALSE (keep the job and retry it again later!)
	//so define any positive status codes as TRUE
	if(defaultPolicy.size() == 0){
		defaultPolicy[HTTPResponse::HTTP_OK] = false; //job done - all ok - no need to rety!
		defaultPolicy[HTTPResponse::HTTP_BAD_REQUEST] = true; //if its our fault - dont try again - would fail every time anyway
		defaultPolicy[HTTPResponse::HTTP_UNAUTHORIZED] = true; //will always fail
		defaultPolicy[HTTPResponse::HTTP_FORBIDDEN] = true; //will always fail
		defaultPolicy[HTTPResponse::HTTP_GONE] = true; //will always fail
	}
}

void ofxUserContentUpload::update(){
	while(executedJobs.size()){
		lock();
		JobExecutionResult n = executedJobs[0];
		ofNotifyEvent(eventJobExecuted, n, this);
		executedJobs.erase(executedJobs.begin());
		unlock();
	}
}

void ofxUserContentUpload::draw(int x, int y){

	int nPending = 0;
	lock();
	nPending = pendingApiRequests.size();
	unlock();

	string msg = "ofxUserContentUpload: \n" 
	"  Num Pending: " + ofToString(numPendingWhenLastChecked + nPending) + "\n"
	"  Num Pending Retry: " + ofToString(numFailedWhenLastChecked) + "\n" +
	"  Num Executed OK so far: " + ofToString(numExecutedOkJobs) + "\n" +
	"  Num Executed & Failed so far: " + ofToString(numExecutedFailedJobs);

	ofDrawBitmapStringHighlight(msg, x, y);
}


void ofxUserContentUpload::setup(const string &storageDir, FailedJobPolicy retryPolicy){

	this->retryPolicy = retryPolicy;
	this->storageDir = storageDir;
	ofDirectory::createDirectory(PENDING_JOBS_LOCAL_PATH, true, true);
	ofDirectory::createDirectory(FAILED_PENDING_JOBS_LOCAL_PATH, true, true);

	executeJobsRate = 1.0; //reasonable default
	failJobSkipRetryFactor = 20;
	timeOut = 20;

	startThread();
}


void ofxUserContentUpload::addJob(Job & job){

	ofLogNotice("ofxUserContentUpload") << "adding a new job '" << job.jobID << "'.";
	lock();
	pendingApiRequests.push_back(job);
	unlock();
}


void ofxUserContentUpload::threadedFunction(){

	int c = 0;

	while(isThreadRunning()){

		ofSleepMillis(executeJobsRate * 500); //sleep N/2 seconds b4 trying to execute the next one

		while(pendingApiRequests.size()){
			lock(); //////////////////////////////////////////////////////////////////////////////////
			Job & j = pendingApiRequests[0];
			saveJobToDisk(j, false);
			pendingApiRequests.erase(pendingApiRequests.begin());
			unlock(); //////////////////////////////////////////////////////////////////////////////
		}

		executeNextPendingJob(false); //lets exec a job from the pending list

		ofSleepMillis(executeJobsRate * 500); //sleep N/2 seconds b4 trying to execute the next FAILED job

		if(c%failJobSkipRetryFactor == 0){ //once every N times, we try to execute failed jobs
			executeNextPendingJob(true); //and then lets try run one from the failed list
		}
		c++;
	}

	ofLogNotice("ofxUserContentUpload") << "exiting ofxUserContentUpload thread!";
}


void ofxUserContentUpload::saveJobToDisk(const Job & j, bool failedDir){
	ofxXmlSettings xml;
	string fn = fileNameForJob(j, failedDir);
	xml.load(fn);

	xml.addTag("ofxUserContentJob");
	xml.pushTag("ofxUserContentJob");

	xml.addTag("config");
	xml.pushTag("config");
		xml.addValue("host", j.host);
		xml.addValue("port", j.port);
		xml.addValue("jobID", j.jobID);
		xml.addValue("timeStamp", j.timeStamp);
		xml.addValue("verbose", j.verbose);
		xml.addValue("numTries", j.numTries);
	xml.popTag();

	xml.addTag("fields");
	xml.pushTag("fields");
	int c = 0;
	for(auto f : j.formFields){
		xml.setValue("field", "", c);
		xml.setAttribute("field", "fieldName", f.first, c);
		xml.setAttribute("field", "fieldValue", f.second, c);
		c++;
	}
	xml.popTag();

	xml.addTag("files");
	xml.pushTag("files");
	c = 0;
	for(auto f : j.fileFields){
		xml.setValue("file", "", c);
		xml.setAttribute("file", "fileFieldName", f.first, c);
		xml.setAttribute("file", "filePath", f.second.first, c);
		xml.setAttribute("file", "mimeType", f.second.second, c);
		c++;
	}
	xml.popTag();

	xml.save(fn);
}


bool ofxUserContentUpload::loadJobFromDisk(const string & path, Job & job){

	ofxXmlSettings xml;
	xml.load(path);
	xml.pushTag("ofxUserContentJob");

	xml.pushTag("config");
	job.host = xml.getValue("host", "");
	job.port = xml.getValue("port", 80);
	job.jobID = xml.getValue("jobID", "missing JOB ID!");
	job.timeStamp = xml.getValue("timeStamp", (int)ofGetUnixTime());
	job.verbose = xml.getValue("verbose", false);
	job.numTries = xml.getValue("numTries", 0);

	bool parseOK = true;
	if(job.host.size() == 0){
		parseOK = false;
	}
	xml.popTag();

	xml.pushTag("fields");
	int n = xml.getNumTags("field");
	for(int i = 0; i < n; i++){
		string fieldName = xml.getAttribute("field", "fieldName", "", i);
		string fieldValue = xml.getAttribute("field", "fieldValue", "", i);
		if(fieldName.size() == 0 || fieldValue.size() == 0){
			parseOK = false;
		}else{
			job.formFields[fieldName] = fieldValue;
		}
	}
	xml.popTag();

	xml.pushTag("files");
	n = xml.getNumTags("file");
	for(int i = 0; i < n; i++){
		string fileName = xml.getAttribute("file", "fileFieldName", "", i);
		string filePath = xml.getAttribute("file", "filePath", "", i);
		string mimeType = xml.getAttribute("file", "mimeType", "", i);
		if(fileName.size() == 0 || filePath.size() == 0 || mimeType.size() == 0){
			parseOK = false;
		}else{
			job.fileFields[fileName] = std::make_pair(filePath, mimeType);
		}
	}
	xml.popTag();

	return parseOK;
}


string ofxUserContentUpload::getNewUUID(){
	static char alphabet[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
	string s;
	for(int i = 0; i < 8; i++) s += ofToString((char)alphabet[(int)floor(ofRandom(16))]);
	s += "-";
	for(int i = 0; i < 4; i++) s += ofToString((char)alphabet[(int)floor(ofRandom(16))]);
	s += "-4";
	for(int i = 0; i < 3; i++) s += ofToString((char)alphabet[(int)floor(ofRandom(16))]);
	s += "-a";
	for(int i = 0; i < 3; i++) s += ofToString((char)alphabet[(int)floor(ofRandom(16))]);
	s += "-";
	for(int i = 0; i < 12; i++) s += ofToString((char)alphabet[(int)floor(ofRandom(16))]);
	return s;
}


bool ofxUserContentUpload::executeNextPendingJob(bool fromFailedFolder){

	ofDirectory d;
	d.allowExt("job"); //list .job files form disk

	if(fromFailedFolder){
		d.listDir(ofToDataPath(FAILED_PENDING_JOBS_LOCAL_PATH));
		numPendingWhenLastChecked = d.size();
	}else{
		d.listDir(ofToDataPath(PENDING_JOBS_LOCAL_PATH));
		numFailedWhenLastChecked = d.size();
	}
	d.sort();

	bool jobLoadOk = false;
	bool jobExecOK = false;
	Job j;
	if(d.size() > 0){

		string fp;
		string fileName;

		if(!fromFailedFolder){
			fp = d.getPath(0);
			fileName = d.getName(0);
		}else{ //we randomly pick one for failed jobs, so that we dont get stuck on the same one forever
			int num = floor(ofRandom(d.size()));
			fp = d.getPath(num);
			fileName = d.getName(num);
		}

		jobLoadOk = loadJobFromDisk(fp, j);

		if(jobLoadOk){
			//ofLogNotice("ofxUserContentUpload") << "About to Execute API job: '" << CooperHewittAPI::toString(j.type) << "' file: " << fileName;
			JobExecutionResult r;
			jobExecOK = executeJob(j, r.serverResponse, r.serverStatusCode, r.errorDescription);

			if(jobExecOK){
				ofLogNotice("ofxUserContentUpload") << "Delete Job '" << j.jobID << "'  file: '" << fileName << "'";
				ofFile::removeFile(fp);
				numExecutedOkJobs++;
			}else{
				numExecutedFailedJobs++;
				if(fromFailedFolder){
					if (j.numTries > maxJobRetries){
						ofLogError("ofxUserContentUpload") << "JOB FAILED AGAIN '" << j.jobID << "' - FOR THE LAST TIME! (" << j.numTries << ") deleting it '" << fileName << "'";
						ofFile::removeFile(fp);
						deleteFilesForJob(j); //remove job-related files too 
					}else{
						ofLogError("ofxUserContentUpload") << "JOB FAILED AGAIN '" << j.jobID << "'  - failed " << j.numTries << " times so far (max " << maxJobRetries <<  "). '" << fileName << "'";
						ofFile::removeFile(fp);
						j.numTries++;
						saveJobToDisk(j, true); //save it back into the fail dir
					}
				}else{
					ofLogError("ofxUserContentUpload") << "JOB FAILED '" << j.jobID << "' (load:" << jobLoadOk << " exec:"<< jobExecOK << ") Moving job to failed dir: '" << fileName << "'";
					ofFile::moveFromTo(fp, string(FAILED_PENDING_JOBS_LOCAL_PATH) + "/" + fileName); //transfer job fom PENDING to FAILED
				}
			}

			r.jobID = j.jobID;
			r.isJobFresh = !fromFailedFolder;
			r.ok = jobExecOK;
			lock();
			executedJobs.push_back(r);
			unlock();
		}else{
			ofLogError("ofxUserContentUpload") << "failed to load job from file '" << fileName << "'";
		}
	}
	d.close();

	return jobLoadOk && jobExecOK;
}


bool ofxUserContentUpload::executeJob(const Job & j,
									  string & serverResponse, 
									  HTTPResponse::HTTPStatus & serverStatus,
									  string & errorDescription
									  ){

	ofLogNotice("ofxUserContentUpload") << "░░░░░░░░░░░░░░░░░░░ Starting Job: \"" << j.jobID << "\" ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░";

	HTTPResponse::HTTPStatus statusCode = HTTPResponse::HTTPStatus(-1);

	HttpForm f = HttpForm( j.host , j.port);
	for(auto ff : j.formFields){
		f.addString(ff.first, ff.second);
	}

	for(auto ff : j.fileFields){
		f.addFile(ff.first, ff.second.first, ff.second.second);
	}

	HttpFormManager fm;
	fm.setTimeOut(timeOut);
	fm.setAcceptString("*/*");
	fm.setVerbose(false);

	//TODO proxy!

	HttpFormResponse r = fm.submitFormBlocking( f );

	string serverMsg;
	statusCode = analyzeStatus(r, serverMsg, j.verbose);
	printStatus(j.jobID, r, serverMsg, statusCode);

	if(j.verbose){
		ofLogNotice("ofxUserContentUpload") << "░░░░░░░░░░░░░░░░░░░ Job Executed \"" << j.jobID << "\" ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░\nVerbose Summary : \n" << r.toString();
	}else{
		ofLogNotice("ofxUserContentUpload") << "░░░░░░░░░░░░░░░░░░░ Job Executed \"" << j.jobID << "\" ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░\nServer Response : " << r.responseBody;
	}

	serverResponse = r.responseBody;
	serverStatus = statusCode;
	errorDescription = serverMsg + " | " + r.reasonForStatus;

	bool shouldRetryJoblater = shouldRetryJobLater(statusCode);
	if(!shouldRetryJoblater){
		deleteFilesForJob(j);
	}else{
		ofLogError("ofxUserContentUpload") << "Job failed '" << j.jobID << "', we will retry again later.";
	}

	return !shouldRetryJoblater;
}


void ofxUserContentUpload::deleteFilesForJob(const Job & job){
	for(auto file : job.fileFields){ //delete all uploaded files - job will not be retried
		if(file.second.first.size()){
			ofLogNotice("ofxUserContentUpload") << "Removing user content file at \"" << file.second.first << "\"" << " attached to JOB \"" << job.jobID << "\"";
			ofFile::removeFile(file.second.first, true);
		}
	}
}


string ofxUserContentUpload::fileNameForJob(const Job& j, bool failedDir){
	string pp;
	if (failedDir){
		pp = FAILED_PENDING_JOBS_LOCAL_PATH;
	}else{
		pp = PENDING_JOBS_LOCAL_PATH;
	}
	return pp + "/t" + ofToString(j.timeStamp) + "_" + getFileSystemSafeString(j.jobID) + "_" + getNewUUID() + ".job";
}


bool ofxUserContentUpload::shouldRetryJobLater(HTTPResponse::HTTPStatus c){

	auto it = retryPolicy.find(c);
	if(it == retryPolicy.end()){
		return true; //if not defined in the policy, we retry later!
	}else{
		return it->second;
	}
}


HTTPResponse::HTTPStatus ofxUserContentUpload::analyzeStatus(HttpFormResponse & r, string &serverMessage, bool verbose){

//TODO parse server response with user lambda?

//	if(r.responseBody.size()){
//		ofxJSONElement myJson;
//		bool parseOk = myJson.parse(r.responseBody);
//		status = toStatusCode(r.status);
//		if(parseOk){
//			if (myJson.isObject()){
//				if (myJson["error"].isObject()){
//					if (myJson["error"]["message"].isString()){
//						serverMessage = myJson["error"]["message"].asString();
//					}
//				}
//			}
//			if (myJson.isObject()){
//				if (myJson["error"].isObject()){
//					if (myJson["error"]["code"].isInt()){
//						int errStatusCode = myJson["error"]["code"].asInt();
//						status = toStatusCode(errStatusCode);
//					}
//				}
//			}
//
//			if(verbose){
//				Json::StyledWriter styledWriter;
//				ofLogNotice() << styledWriter.write(myJson);
//			}
//
//		}else{
//			ofLogError("ofxUserContentUpload") << "Cant parse server response!";
//		}
	if((int)r.status != -1){
		serverMessage = HTTPResponse::getReasonForStatus(r.status);
	}else{
		serverMessage = r.reasonForStatus;
	}
	return r.status;
}


void ofxUserContentUpload::printStatus(const string & jobID, HttpFormResponse & r, const string & serverMsg, HTTPResponse::HTTPStatus status){

	switch (status) {
		case HTTPResponse::HTTP_OK:
			ofLogNotice("ofxUserContentUpload") << "Job \"" << jobID << "\" \"" << r.url << "\"" << " FINISHED OK!! '"
				<< ofToString(status) << "' took " << r.totalTime << " sec";
			break;

		default:
			ofLogError("ofxUserContentUpload") << "Job \"" << jobID << "\" \"" << r.url << "\"" << " FAILED!!   Status: '"
				<< ofToString(status) << "'  Reason: '" << serverMsg << "' http("
				<< r.reasonForStatus << ") took " << r.totalTime << " sec";
			break;
	}
}


string ofxUserContentUpload::getFileSystemSafeString(const string & input){
	static char invalidChars[] = {'?', '\\', '/', '*', '<', '>', '"', ';', ':', '#' };
	int howMany = sizeof(invalidChars) / sizeof(invalidChars[0]);
	char replacementChar = '_';
	string output = input;
	for(int i = 0; i < howMany; i++){
		std::replace( output.begin(), output.end(), invalidChars[i], replacementChar);
	}
	return output;
}

