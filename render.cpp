#include <Bela.h>
#include <fstream>
#include <cmath>
#include <SampleStream.h>

#define NUM_CHANNELS 4    // NUMBER OF CHANNELS IN THE FILE
#define BUFFER_LEN 44100   // BUFFER LENGTH
#define NUM_STREAMS 1	// NUMBER OF STREAMS TO PLAY THROUGH

SampleStream *sampleStream[NUM_STREAMS];
AuxiliaryTask gFillBuffersTask;
int gStopThreads = 0;
int gTaskStopped = 0;
int gCount = 0;

int gStreamPlaying = 0;		// bool to tell state of streaming
float gFadeTime = 2.0;		// time to fade in and out of audio stream
int gMovement = 0;			// bool to register sensor movement
int gCountdown = 0;			// object to hold countdown performance
int gCountdownValue = 5;	// countdown time in seconds
int gClock = 0;				// clock to convert samples to seconds
int gRunTime = 0;			// total run time clock
int gOpenTime = 54000;		// wanted runtime in seconds before shutdown (15 hours) (08:00 - 23:00)
float gVisitorCount = 0.0;	// float to count and store data on number of visitors
int gAudioFramesPerAnalogFrame = 0;

void fillBuffers(void*) {
    for(int i=0;i<NUM_STREAMS;i++) {
        if(sampleStream[i]->bufferNeedsFilled())
            sampleStream[i]->fillBuffer();
    }
}

bool setup(BelaContext *context, void *userData)
{

    for(int i=0;i<NUM_STREAMS;i++) {
        sampleStream[i] = new SampleStream("quad_test.wav", NUM_CHANNELS,BUFFER_LEN);
    }

    // Initialise auxiliary tasks
	if((gFillBuffersTask = Bela_createAuxiliaryTask(&fillBuffers, 90, "fill-buffer")) == 0)
		return false;
	
	// Setup analog frames
	if(context->analogFrames)
		gAudioFramesPerAnalogFrame = context->audioFrames / context->analogFrames;

	return true;
}

void render(BelaContext *context, void *userData)
{
	// check if buffers need filling
    Bela_scheduleAuxiliaryTask(gFillBuffersTask);  
	
    for(unsigned int n = 0; n < context->audioFrames; n++) {
    	
    	// GET ANALOG SENSOR INPUT
    	if(gAudioFramesPerAnalogFrame && !(n % gAudioFramesPerAnalogFrame)) {
            float input = analogRead(context, n, 0);
    		gMovement = (input > 0.4) ? 1 : 0;
        }
        
        // PLAY/PAUSE AUDIO STREAM
		if (gMovement && !gStreamPlaying) {
    		sampleStream[0]->togglePlaybackWithFade(gFadeTime);
    		gStreamPlaying = 1;
    		gVisitorCount++;
		} else if (!gMovement && gStreamPlaying && gCountdown == 0) {
			sampleStream[0]->togglePlaybackWithFade(gFadeTime);
			gStreamPlaying = 0;
		}
    	
		// COUNTDOWN
		// reset countdown if there is movement
		if(gMovement) { gCountdown = gCountdownValue; }
    
    	if(++gClock >= context->audioSampleRate) {
    		gClock = 0;
    		
    		// total runtime and shutdown after opening time
    		gRunTime++;
    		if (gRunTime > gOpenTime) { system("/root/Bela/scripts/halt_board.sh"); }
			
			// no movement countdown
    		if (gCountdown > 0) { gCountdown--; }
    	}
    		
        for(int i=0;i<NUM_STREAMS;i++) {
            // process frames for each sampleStream objects (get samples per channel below)
            sampleStream[i]->processFrame();
        }

    	for(unsigned int channel = 0; channel < NUM_CHANNELS; channel++) {

            float out = 0;
            for(int i=0;i<NUM_STREAMS;i++) {
                // get samples for each channel from each sampleStream object
                out += sampleStream[i]->getSample(channel);
            }
            
            audioWrite(context, n, channel, out);

    	}
    }
}


// Function to add visitor count to text file
void logVisitorCount(const char* filename, float data) {
    std::ofstream outputFile(filename, std::ios::app);
    if (outputFile.is_open()) {
        outputFile << data << std::endl;
        outputFile.close();
    }
}


void cleanup(BelaContext *context, void *userData)
{
    for(int i=0;i<NUM_STREAMS;i++) { delete sampleStream[i]; }
    logVisitorCount("daily_visitors.txt", gVisitorCount);
}
