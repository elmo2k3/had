#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>


int main(int argc, char *argv[]) {
	enum sun mode;
	enum out output;

	if (argc < 4 || argc > 6) goto usage;


	if (strcmp(argv[1], "rise") == 0) mode = RISE;
	else if (strcmp(argv[1], "set") == 0) mode = SET;
	else goto usage;

	if (argc >= 6) {
		if (strcmp(argv[5], "human") == 0) output = HUMAN;
		else if (strcmp(argv[5], "unix") == 0) output = UNIX;
		else goto usage;
	}
	else {
		output = HUMAN;
	}

	int timezone = (argc >= 5) ? atoi(argv[4]) : 0;
	double lat = deg2rad(strtod(argv[2], NULL));
	double lon = deg2rad(strtod(argv[3], NULL));
	double time = sun(mode, lat, lon, timezone);
	double intpart;

	switch (output) {
		case HUMAN:
			printf("%d:%02d\n", (int) floor(time), (int) (modf(time, &intpart)*60));
			break;
		case UNIX:
			printf("%d\n", (int) round(time*60*60));
			break;
	}

	return 0;

usage:
	fprintf(stderr, "usage: sun MODE LAT LON [TIMEZONE] [OUTPUT]\n");
	fprintf(stderr, "  MODE\t\t'rise' or 'set'\n");
	fprintf(stderr, "  LAT, LON\tthe geografical postition in degree\n");
	fprintf(stderr, "  TIMEZONE\tan integer describing the timezone offset\n");
	fprintf(stderr, "  OUTPUT\t'human' (default) or 'unix'\n");

	return -1;
}
