#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* CURL for reader-thread */
#include <curl/curl.h>
#include <dirent.h>

/* POSIX Threads */
#define _MULTI_THREADED
#include <pthread.h>
#include <signal.h>

/* fmod for player-thread */
#include "fmodapi44407linux/api/inc/fmod.h"
#include "fmodapi44407linux/api/inc/fmod_errors.h"
#include "fmodapi44407linux/examples/common/wincompat.h"

const char * STATION_URL="http://203.150.225.77:8000";
const int intial_buffering_delay = 3;
const int fs_buffer_size = 1024;
const int playback_buffer_size = 4096;

/****************************************************
 *
 *  reader-thread functions for fetching stream
 *
 ****************************************************/  
void sighand(int signo)
{
	pthread_t self = pthread_self();
 
	return;
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{

	int written=0;

	written = fwrite(ptr, size, nmemb, (FILE *)stream);
	printf("written=%d\tsize=%d\tnmemb=%d\ttotal=%d\n", written, size, nmemb, size*nmemb);

	return nmemb;
}

int curl_main(void)
{
	CURL *curl_handle;
	static const char *bodyfilename = "wave.dat";
	FILE *bodyfile;

	curl_global_init(CURL_GLOBAL_ALL);

	/* init the curl session */
	curl_handle = curl_easy_init();

	/* set URL to get */
	curl_easy_setopt(curl_handle, CURLOPT_URL, STATION_URL);

	//struct curl_slist *headers=NULL;
	//headers = curl_slist_append(headers, "Icy-MetaData:1"); 

	/* no progress meter please */
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);

	/* send all data to this function  */
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);

	bodyfile = fopen(bodyfilename,"w");
	if (bodyfile == NULL) {
		curl_easy_cleanup(curl_handle);
		return -1;
	}

	/* we want the headers to this file handle */
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, bodyfile);

	/* pass our list of custom made headers */
	//curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers); 

	/*
	 * Notice here that if you want the actual data sent anywhere else but
	 * stdout, you should consider using the CURLOPT_WRITEDATA option.  */

	/* get it! */
	curl_easy_perform(curl_handle);

	/* close the header file */
	fclose(bodyfile);

	/* cleanup curl stuff */
	curl_easy_cleanup(curl_handle);

	return 0;
}


/****************************************************
 *
 *  fmod functions for local playback
 *
 ****************************************************/  

void ERRCHECK(FMOD_RESULT result)
{
	if (result != FMOD_OK)
	{
		printf("FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
		exit(-1);
	}
}


/*
TIPS:

1. use F_CALLBACK.  Do NOT force cast your own function to fmod's callback type.
2. return FMOD_ERR_FILE_NOTFOUND in open as required.
3. return number of bytes read in read callback.  Do not get the size and count 
around the wrong way in fread for example, this would return 1 instead of the number of bytes read.

QUESTIONS:

1. Why does fmod seek to the end and read?  Because it is looking for ID3V1 tags.  
Use FMOD_IGNORETAGS in FMOD_System_CreateSound / FMOD_System_CreateStream if you don't like this behaviour.
*/

FMOD_RESULT F_CALLBACK myopen(const char *name, int unicode, unsigned int *filesize, void **handle, void **userdata)
{
	if (name)
	{
		FILE *fp;

		fp = fopen(name, "rb");
		if (!fp)
		{
			return FMOD_ERR_FILE_NOTFOUND;
		}

#if 0
		fseek(fp, 0, SEEK_END);
		*filesize = ftell(fp);
#else
		*filesize = 2147483647;
#endif
		fseek(fp, 0, SEEK_SET);

		*userdata = (void *)0x12345678;
		*handle = fp;
	}

	return FMOD_OK;
}

FMOD_RESULT F_CALLBACK myclose(void *handle, void *userdata)
{
	if (!handle)
	{
		return FMOD_ERR_INVALID_PARAM;
	}

	fclose((FILE *)handle);

	return FMOD_OK;
}

FMOD_RESULT F_CALLBACK myread(void *handle, void *buffer, unsigned int sizebytes, unsigned int *bytesread, void *userdata)
{
	if (!handle)
	{
		return FMOD_ERR_INVALID_PARAM;
	}

	if (bytesread)
	{
		*bytesread = (int)fread(buffer, 1, sizebytes, (FILE *)handle);

		if (*bytesread < sizebytes)
		{
			return FMOD_ERR_FILE_EOF;
		}
	}

	return FMOD_OK;
}

FMOD_RESULT F_CALLBACK myseek(void *handle, unsigned int pos, void *userdata)
{
	if (!handle)
	{
		return FMOD_ERR_INVALID_PARAM;
	}

	fseek((FILE *)handle, pos, SEEK_SET);

	return FMOD_OK;
}



/****************************************************
 *
 *  main function. Handles console UI as well.
 *
 ****************************************************/

int main(int argc, char *argv[])
{
	struct sigaction	actions;
	pthread_t		playerthread;

	FMOD_SYSTEM		*system;
	FMOD_SOUND		*sound;
	FMOD_CHANNEL		*channel = 0;
	FMOD_RESULT		result;
	int			key;
	unsigned int		version;

	memset(&actions, 0, sizeof(actions));
	sigemptyset(&actions.sa_mask);
	actions.sa_flags = 0;
	actions.sa_handler = sighand;

	result = sigaction(SIGALRM,&actions,NULL);

	/* Initiate the request-thread using CURL */
	pthread_create(&playerthread, NULL, (void*)curl_main, NULL);

	/* Delay to allow for station-connect and buffering */
	sleep(intial_buffering_delay);

	/*
	   Create a System object and initialize.
	   */
	result = FMOD_System_Create(&system);
	ERRCHECK(result);

	result = FMOD_System_GetVersion(system, &version);
	ERRCHECK(result);

	if (version < FMOD_VERSION)
	{
		printf("Error!  You are using an old version of FMOD %08x.  This program requires %08x\n", version, FMOD_VERSION);
		return 0;
	}

	result = FMOD_System_Init(system, 1, FMOD_INIT_NORMAL, NULL);
	ERRCHECK(result);

	result = FMOD_System_SetFileSystem(system, myopen, myclose, myread, myseek, 0, 0, fs_buffer_size);
	ERRCHECK(result);

	result = FMOD_System_SetStreamBufferSize(system, playback_buffer_size, FMOD_TIMEUNIT_RAWBYTES);

	result = FMOD_System_CreateStream(system,
					  "./wave.dat",
					  FMOD_HARDWARE | FMOD_2D | FMOD_IGNORETAGS,
					  0,
					  &sound);
	ERRCHECK(result);

	printf("====================================================================\n");
	printf("  Playing %s\n", STATION_URL);
	printf("====================================================================\n");
	printf("\n");
	printf("Press space to pause, Esc to quit\n");
	printf("\n");

	/*
	   Play the sound.
	 */

	result = FMOD_System_PlaySound(system, FMOD_CHANNEL_FREE, sound, 0, &channel);
	ERRCHECK(result);

	/*
	   Main loop.
	 */
	do
	{
		if (kbhit())
		{
			key = getch();

			switch (key)
			{
				case ' ' :
					{
						int paused;
						FMOD_Channel_GetPaused(channel, &paused);
						FMOD_Channel_SetPaused(channel, !paused);
						break;
					}
			}
		}

		FMOD_System_Update(system);

		if (channel)
		{
			unsigned int ms;
			unsigned int lenms;
			int          playing;
			int          paused;

			FMOD_Channel_IsPlaying(channel, &playing);
			if ((result != FMOD_OK) && (result != FMOD_ERR_INVALID_HANDLE) && (result != FMOD_ERR_CHANNEL_STOLEN))
			{
				ERRCHECK(result);
			}

			result = FMOD_Channel_GetPaused(channel, &paused);
			if ((result != FMOD_OK) && (result != FMOD_ERR_INVALID_HANDLE) && (result != FMOD_ERR_CHANNEL_STOLEN))
			{
				ERRCHECK(result);
			}

			result = FMOD_Channel_GetPosition(channel, &ms, FMOD_TIMEUNIT_MS);
			if ((result != FMOD_OK) && (result != FMOD_ERR_INVALID_HANDLE) && (result != FMOD_ERR_CHANNEL_STOLEN))
			{
				ERRCHECK(result);
			}

			result = FMOD_Sound_GetLength(sound, &lenms, FMOD_TIMEUNIT_MS);
			if ((result != FMOD_OK) && (result != FMOD_ERR_INVALID_HANDLE) && (result != FMOD_ERR_CHANNEL_STOLEN))
			{
				ERRCHECK(result);
			}

			printf("Time %02d:%02d:%02d/%02d:%02d:%02d : %s\r", ms / 1000 / 60, ms / 1000 % 60, ms / 10 % 100, lenms / 1000 / 60, lenms / 1000 % 60, lenms / 10 % 100, paused ? "Paused " : playing ? "Playing" : "Stopped");
			fflush(stdout);
		}

		Sleep(10);

	} while (key != 27);

	printf("\n");

	//pthread_join(playerthread, NULL);
	pthread_kill(playerthread, SIGALRM);

	/*
	   Shut down
	   */
	result = FMOD_Sound_Release(sound);
	ERRCHECK(result);
	result = FMOD_System_Close(system);
	ERRCHECK(result);
	result = FMOD_System_Release(system);
	ERRCHECK(result);

	return 0;
}



