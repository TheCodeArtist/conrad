#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>

/* for getopt_long() */
#include <getopt.h>

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

#define DEBUG	0

#define debug(fmt, ...) \
	do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0)

const char *bodyfilename = "wave.dat";
const int intial_buffering_delay = 3;
const int fs_buffer_size = 1024;
const int playback_buffer_size = 4096;

char *station_url = "http://203.150.225.77:8000";

CURL *curl_handle;
FILE *bodyfile;
bool stop_signal_received = FALSE;

/****************************************************
 *
 *  reader-thread functions for fetching stream
 *
 ****************************************************/  
void sighand(int signo)
{
	if (signo == SIGALRM) {
		debug("Received signal 0x%x\n", signo);
		stop_signal_received = TRUE;
	}

	return;
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{

	int written=0;

	written = fwrite(ptr, size, nmemb, (FILE *)stream);
	debug("written=%d\tsize=%d\tnmemb=%d\ttotal=%d\n", written, size, nmemb, size*nmemb);

	/* Returning -EINVAL terminates the curl_easy_perform() */
	if(stop_signal_received) return -EINVAL;

	return nmemb;
}

int curl_main(void)
{
	curl_global_init(CURL_GLOBAL_ALL);

	/* init the curl session */
	curl_handle = curl_easy_init();

	/* set URL to get */
	curl_easy_setopt(curl_handle, CURLOPT_URL, station_url);

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

static void usage(FILE *fp, int argc, char **argv)
{
	fprintf(fp,
			"Usage: conrad -s <url> [options]\n\n"
			"Options:\n"
			"-s | --station <url>   Radio station URL \n"
			"-h | --help            Print this message\n"
			"", argv[0]);
}


static const char short_options[] = "hs:";

static const struct option
long_options[] = {
	{ "station", required_argument, NULL, 's' },
	{ 0, 0, 0, 0 }
};


/****************************************************
 *
 *  main function. Handles console UI as well.
 *
 ****************************************************/

int main(int argc, char *argv[])
{
	struct sigaction	actions;
	pthread_t		fetcherthread;

	FMOD_SYSTEM		*system;
	FMOD_SOUND		*sound;
	FMOD_CHANNEL		*channel = 0;
	FMOD_RESULT		result;
	int			key, paused;
	unsigned int		version;

	for (;;) {
		int idx;
		int c;

		c = getopt_long(argc, argv,
				short_options, long_options, &idx);

		if (-1 == c)
			break;

		switch (c) {
			case 0:
				/* getopt_long() flag */
				break;

			case 's':
				/* getopt_long() flag */
				station_url = optarg;
				printf("station_url=%s\n", station_url);
				break;

			case 'h':
				usage(stdout, argc, argv);
				exit(EXIT_SUCCESS);

			default:
				usage(stderr, argc, argv);
				exit(EXIT_FAILURE);
		}
	}



	memset(&actions, 0, sizeof(actions));
	sigemptyset(&actions.sa_mask);
	actions.sa_flags = 0;
	actions.sa_handler = sighand;

	result = sigaction(SIGALRM,&actions,NULL);

	/* Initiate the request-thread using CURL */
	pthread_create(&fetcherthread, NULL, (void*)curl_main, NULL);

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
	printf("  Playing %s\n", station_url);
	printf("====================================================================\n");
	printf("\n");
	printf("Press space to pause, Esc to quit\n");
	printf("\n");

	/* Play the sound */
	result = FMOD_System_PlaySound(system, FMOD_CHANNEL_FREE, sound, 0, &channel);
	ERRCHECK(result);

	/* Main loop */
	do
	{
		if (kbhit())
		{
			key = getch();

			switch (key) {

				case ' ' :
					FMOD_Channel_GetPaused(channel, &paused);
					FMOD_Channel_SetPaused(channel, !paused);
					break;
			}
		}

		FMOD_System_Update(system);

		if (channel)
		{
			unsigned int ms;
			unsigned int lenms;
			int          playing;

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

			printf("[ %s ] %02d:%02d\r",
					paused ? "Paused " : playing ? "Playing" : "Stopped",
					ms / 1000 / 60, ms / 1000 % 60);
			fflush(stdout);
		}

		Sleep(10);

	} while (key != 27);

	printf("\n");

	/* If control has reached this point, then the user wants to quit the program.
	 * Hence signal termination to the other thread.
	 */
	pthread_kill(fetcherthread, SIGALRM);

	/* Wait until the other thread finishes clean-up and terminates */
	pthread_join(fetcherthread, NULL);

	/*
	 * Shut down FMODex
	 */
	result = FMOD_Sound_Release(sound);
	ERRCHECK(result);
	result = FMOD_System_Close(system);
	ERRCHECK(result);
	result = FMOD_System_Release(system);
	ERRCHECK(result);

	return 0;
}



