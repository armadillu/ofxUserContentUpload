//
//  ofxUserContentUpload.h
//  CollectionTable
//
//  Created by Oriol Ferrer Mesià on 01/02/15.
//
//

#pragma once

#include "ofMain.h"
#include "HttpFormManager.h"

#define PENDING_JOBS_LOCAL_PATH					(storageDir + "/pending")
#define FAILED_PENDING_JOBS_LOCAL_PATH			(storageDir + "/failed")


class ofxUserContentUpload: public ofThread{

public:

	struct Job{

		void createJob(const string & host, int port, string jobID = ""){
			this->host = host;
			this->port = port;
			this->jobID = jobID;
		}

		void addFile(const string & fileName, const string & filePath, string mimeType = "text/plain"){
			if(fileFields.find(fileName) == fileFields.end()){
				fileFields[fileName] = std::make_pair(filePath, mimeType);
			}else{
				ofLogError("ofxUserContentUpload::Job") << "cant addFile(), already exists! " << fileName;
			}
		}

		void addStringField(const string & filedName, const string & fieldValue){
			if(formFields.find(filedName) == formFields.end()){
				formFields[filedName] = fieldValue;
			}else{
				ofLogError("ofxUserContentUpload::Job") << "cant addStringField(), already exists! " << filedName;
			}
		}

		string host;
		int port;
		string jobID; //you will get this ID back when the job is done

		map<string, string>						formFields; //fieldName-value
		map<string, std::pair<string, string>>	fileFields; //filedName : <filepath, mimetype> (ie "text/plain")

		int timeStamp;
		bool verbose;
		int numTries = 0;

		Job(){
			timeStamp = ofGetUnixTime();
		}
	};

	typedef map<HTTPResponse::HTTPStatus, bool>	 FailedJobPolicy;

	struct JobExecutionResult{
		bool ok;
		bool isJobFresh; //ie not a retry, the first time we try
		string jobID;
		string serverResponse;
		string errorDescription;
		HTTPResponse::HTTPStatus serverStatusCode;
	};

	ofxUserContentUpload();
	~ofxUserContentUpload();

	void setup(const string & storageDir, FailedJobPolicy retryPolicy = ofxUserContentUpload::defaultPolicy);

	void addJob(Job & job);

	void update();
	void draw(int x, int y);

	void setMaxNumberRetries(int n){ maxJobRetries = n;} //if a job failed to send (and keeps failing)it will only be re-tried N times at max
	int& getMaxNumRetries(){return maxJobRetries;}
	void setTimeOut(float timeOut_){timeOut = timeOut_;}
	float& getTimeOut(){return timeOut;}
	float& getExecuteJobsRate(){return executeJobsRate;}
	int& getFailJobSkipRetryFactor(){return failJobSkipRetryFactor;}

	ofEvent<JobExecutionResult> eventJobExecuted;

	static FailedJobPolicy defaultPolicy;


	static string getNewUUID();
	static string getFileSystemSafeString(const string & input);
	static string getUniqueFilename(const string & name); //"unique" filename generator


private:

	FailedJobPolicy retryPolicy;

	vector<Job> pendingApiRequests;

	void threadedFunction();

	void saveJobToDisk(const Job &, bool failedDir);
	bool loadJobFromDisk(const string & path, Job & job);
	bool executeNextPendingJob(bool fromFailedFolder);
	bool executeJob(const Job &,
					string & serverResponse,
					HTTPResponse::HTTPStatus & serverStatus,
					string & errorDescription
					);

	void printStatus(const string & jobID,
					 HttpFormResponse & r,
					 const string & serverMsg,
					 HTTPResponse::HTTPStatus status);

	string fileNameForJob(const Job &, bool failedDir);
	string	apiToken;
	float timeOut;

	vector<JobExecutionResult> executedJobs;

	float executeJobsRate; //seconds
	int failJobSkipRetryFactor; //N times executeJobsRate

	int numExecutedOkJobs = 0;
	int numExecutedFailedJobs = 0;

	int maxJobRetries = 50;

	string storageDir;

	int numPendingWhenLastChecked = 0;
	int numFailedWhenLastChecked = 0;

	bool shouldRetryJobLater(HTTPResponse::HTTPStatus); //this decides if a job is to give up or retry later if it failed
	HTTPResponse::HTTPStatus analyzeStatus(HttpFormResponse & r, string &serverMessage, bool verbose);

	void deleteFilesForJob(const Job & job);
#ifdef TARGET_WIN32
	const string separator1 = "$$$$$$$$$$$$$$$$$$$  ";
	const string separator2 = "  $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$";
#else
	const string separator1 = "░░░░░░░░░░░░░░░░░░░  ";
	const string separator2 = " ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░";

#endif
};

