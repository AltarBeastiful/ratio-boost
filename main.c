
#include <stdio.h>
#include <time.h>
#include <curl/curl.h>
#include <unistd.h>
#include <stdio.h>
#include <argp.h>
#include "urle.h"	//encode hash
#include "hash.h"	//get torrent meta info
#include "blex.h"	//generate a linked list

const char *argp_program_version =
"ratioboost 1.0";

static char doc[] =
  "Ratio-Boost -- Fakes the amount of data torrent clients report to torrent trackers";

static char args_doc[] = "TORRENTFILE";

static struct argp_option options[] = {
  {"up-speed", 'u', "SPEED", 0,
   "Set upload speed to SPEED (default 30 ko/s)" },
  { 0 }
};

struct arguments
{
  char *torrent_file;
  int up_speed;
};

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
    struct arguments *arguments = state->input;

    switch (key) {
        case 'u':
            arguments->up_speed = arg ? atoi(arg) : 30;
            break;

        case ARGP_KEY_ARG:
            if (state->arg_num >= 3)
                /* Too many arguments. */
                argp_usage (state);

            arguments->torrent_file = arg;
        break;

        case ARGP_KEY_END:
            if (state->arg_num < 1)
                /* Not enough arguments. */
                argp_usage (state);
            break;

        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

//perform connection to the torrent tracker
int tracker_connect(CURL *handle, char *request, struct responce *rdata) {
	
	if (handle == NULL) {

		return 1;
	}

	FILE *output_file = tmpfile();
	//CURLcode res;	
	
	curl_easy_setopt(handle, CURLOPT_URL, request);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, output_file);
	
	//preform the request
	curl_easy_perform(handle);
	load_responce_info(output_file, rdata);
	fclose(output_file);
	
	if (rdata->failure[0] != 0) {
		
		printf("%s\n", rdata->failure);
		
		return 1;

	} else {
		
		printf("seeders = %d leeches = %d update interval = %d\n",rdata->complete, rdata->incomplete, rdata->interval);
	}

	return 0;
}

int main(int argc, char *argv[]) {
	
	struct torrent info = {{0}, {0}, {0}, 0};
	struct responce resp = {{0}, {0}, -1, -1, -1};
	char e_hash[64];	
	char e_peer_id[64];	
	char request[512];
	long uploaded = 0;
	FILE *torrent_file;
    struct arguments arguments;
    time_t now;
	
    /* Default values. */
    arguments.up_speed = 30;

    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    //Open torrent file for processing
    torrent_file = fopen(arguments.torrent_file, "rb");

	if (torrent_file == 0) {

		printf("Error opening file\n");
		
		return 0;
	}
	
	//Load all the required information found in the torrent file to a torrent structure
	int ti = load_torrent_info(torrent_file, &info);

	if (ti != 0) {
		
		printf("invalid torrent file or bencoded responce.\n");
		return 1;
	}

	fclose(torrent_file);
	
	//convert some data to a format needed by the tracker
	urle(e_hash, info.info_hash);
	urle(e_peer_id, (unsigned char *) info.peer_id);

	//prepare the URL, and query string needed for a correct tracker responce.
	sprintf(request, "%s?info_hash=%s&peer_id=%s&port=51413&uploaded=0&downloaded=%lu&left=0&event=started&numwant=1&compact=1", info.url, e_hash, e_peer_id, info.size);	

	//use libcurl to connect to the tracker server and issue a request
	CURL *curl_handle = curl_easy_init();

	int tc = tracker_connect(curl_handle, request, &resp);
	
	//responce failure found exit program
	if (tc == 1) {
		
		return 1;
	}

	//prepare to loop at regular intervals
	struct timespec start;
	struct timespec current;
    int kb_sec = arguments.up_speed;	

	clock_gettime(CLOCK_MONOTONIC, &start);
	
	while (1) {
	
		clock_gettime(CLOCK_MONOTONIC, &current);
		int diff = current.tv_sec - start.tv_sec;

		//resp->interval number of seconds has passed
		if (diff >= resp.interval) {
			
			clock_gettime(CLOCK_MONOTONIC, &start);

            uploaded += (resp.interval) * kb_sec;

			//update request query
            sprintf(request, "%s?info_hash=%s&peer_id=%s&port=51413&uploaded=%ld000&downloaded=%lu&left=0&event=&numwant=1&compact=1", info.url, e_hash, e_peer_id, uploaded, info.size);
	
			//preform the request
			tracker_connect(curl_handle, request, &resp);
			
            float kb = uploaded;
			float mb = kb / 1024;

            time(&now);
            printf("%s: uploaded = %.2f MB @ %d KB/s\n", ctime(&now), mb, kb_sec);
		}

        sleep(resp.interval - diff);
	}

	//free memory used by the curl easy interface
	curl_easy_cleanup(curl_handle);

	return 0;
}
