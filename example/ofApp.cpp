#include "ofApp.h"

void ofApp::setup(){

	upload.setup("ofxUserContentUpload");
	upload.setTimeOut(5);
	upload.getExecuteJobsRate() = 1;
	upload.getFailJobSkipRetryFactor() = 1;
	upload.setMaxNumberRetries(4);

	ofAddListener(upload.eventJobExecuted, this, &ofApp::onJobExecuted);
}


void ofApp::onJobExecuted(ofxUserContentUpload::JobExecutionResult& r){

	ofLogNotice("ofApp") << "onJobExecuted: " << r.jobID;
	ofLogNotice("ofApp") << "serverResponse: " << r.serverResponse;

	if(!r.ok){
		ofLogError("ofApp") << "status code: " << r.serverStatusCode;
		ofLogError("ofApp") << "error description: " << r.errorDescription;
	}
}


void ofApp::update(){

	float dt = 1./60.;
	upload.update();
}


void ofApp::draw(){

	upload.draw(20,20);
}

void ofApp::keyPressed(int key){

	if(key == '1'){
		ofxUserContentUpload::Job job;
		job.createJob("http://192.168.33.10/portrait/submit", 80, "myTest GOOD " + ofToString(counter));
		job.addStringField("email_address1", "oriol@uri.cat");
		job.addStringField("email_address2", "banana@uri.cat");
		job.addStringField("language", "en");
		job.addFile("file", "benotto.jpg");
		job.verbose = false;
		upload.addJob(job);
		counter++;
	}

	if(key == '2'){
		ofxUserContentUpload::Job job;
		job.createJob("http://192.168.33.100/portrait/submit", 80, "myTest BAD " + ofToString(counter));
		job.addStringField("email_address1", "oriol@uri.cat");
		job.addStringField("email_address2", "banana@uri.cat");
		job.addStringField("language", "en");
		job.addFile("file", "benotto.jpg");
		job.verbose = false;
		upload.addJob(job);
		counter++;
	}
}


void ofApp::keyReleased(int key){

}


void ofApp::mouseMoved(int x, int y ){

}


void ofApp::mouseDragged(int x, int y, int button){

}


void ofApp::mousePressed(int x, int y, int button){

}


void ofApp::mouseReleased(int x, int y, int button){

}


void ofApp::windowResized(int w, int h){

}


void ofApp::gotMessage(ofMessage msg){

}


void ofApp::dragEvent(ofDragInfo dragInfo){
	
}

