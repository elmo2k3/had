#include <stdio.h>
#include <stdlib.h>
#include <gd.h>
#include <gdfontl.h>

#include "sma.h"

int
main(int argc, char *argv[])
{
	char buffer[512];
	double pnga, pngm = 0;
	int day = 0, day2 = 0;
	gdImagePtr im;
	FILE *pngout;
	double sma = 0.0;
	double maximum = 0.0;
	double minimum = -274.0;
	double current = 0.0;
	double middle  = -274.0; 
	double mid = 0.0;
	double temp    = 0.0;
  long count = 0;
	int rd;
	int pngx = 0, pngy = 0, white, black, blue, white2, realred, white3;
	char lasttime[32], lastdate[32], datum[32], zeit[32], wert[32];
	FILE *fp;

	printf("Content-Type: text/html\n\n");

	printf("<html>\n");
	printf("<head>\n");
	printf("<script type=\"text/javascript\">\n");
	printf( "var lade = new Image();"
					"lade.src=\"/test.png\";"
					"function Reload() {"
  				"location.href = 'temperature';"
  				"window.setTimeout('Reload()', 45000);"
					"}"
					"function Load() {"
					"document.images.bild.src = lade.src + '?' + Math.random();"
					"window.setTimeout('Reload()', 45000);"
					"}");
	printf("</script>");
	printf("</head>\n");
	printf("<body bgcolor=\"black\" onload=\"Load();\">\n");
	/*printf("<h1>Temperatur:</h2>\n");
*/
	fp = fopen("/svr/database/temperature.current", "r");
	if(fp == 0) {
		printf("<b>Datenbankfehler!<b>");
		printf("<\\html>\n");
		exit(0);
	}

		rd = fread(datum, 11, 1, fp); datum[10] = 0;
		rd = fread(zeit, 10, 1, fp); zeit[8] = 0;
		strcpy(lastdate, datum);
		strcpy(lasttime, zeit);
		fgets(wert, 30, fp); wert[strlen(wert)-1] = 0;
		sscanf(wert, "%lf", &current);
/*		printf("<table>\n");
		printf("\t<tr><td>Aktuelle Temperatur:</td><td>%s°C</td><tr>\n", wert);
		printf("</table>\n");
*/
	fclose(fp);
	fp = fopen("/svr/database/temperature.data", "r");
	if(fp == 0) {
		printf("<b>Datenbankfehler!<b>");
		printf("<\\html>\n");
		exit(0);
	}
	im = gdImageCreate(1235, 720);
	black = gdImageColorAllocate(im,0,0,0);
	white = gdImageColorAllocate(im,0,255,255);
	white3 = gdImageColorAllocate(im,64,64,64);
	white2 = gdImageColorAllocate(im,0,128,64);
	blue = gdImageColorAllocate(im, 32, 32, 32);
	realred = gdImageColorAllocate(im, 192, 192, 0);

	/*printf("<taBle width='400px'>\n");
		printf("<tr>\n");
		printf("\t<th>%s</th><th>%s</th><th>%s</th>\n", "Datum", "Uhrzeit", "Temperatur");
		printf("<tr>\n");
	*/pnga = 0;
	while(!feof(fp)) {
		rd = fread(datum, 11, 1, fp); datum[10] = 0;
		rd = fread(zeit, 10, 1, fp); zeit[8] = 0;
		fgets(wert, 30, fp); wert[strlen(wert)-1] = 0;
		
		if(datum[0] == '2' && datum[1] == '0')
			continue;
		if(datum[0] == '2' && datum[1] == '1')
			continue;
		if(datum[0] == '2' && datum[1] == '2')
			continue;
		if(datum[0] == '2' && datum[1] == '3')
			continue;
		if(datum[0] == '2' && datum[1] == '4')
			continue;
		if(datum[0] == '2' && datum[1] == '5')
			continue;
		if(datum[0] == '2' && datum[1] == '6')
			continue;
		if(datum[0] == '2' && datum[1] == '7')
			continue;
		if(datum[0] == '2' && datum[1] == '8')
			continue;
		if(datum[0] == '2' && datum[1] == '9')
			continue;
		
		if(!feof(fp)) {
			count++;
			sscanf(wert, "%lf", &temp);
			sscanf(datum, "%i", &day2);
			pnga+=temp;
			
			/*
  	   * Simple Moving Average
       */
			if (count == 1)
				sma_init(temp);
			else
				sma_add(temp);	
		
			if((count % 10) == 0) {
				if(day != day2) {
					day = day2;
	
					gdImageLine(im, pngx, 0, pngx, 600, white3);
					gdImageString(im, gdFontGetLarge(), pngx - 6 + (strlen(datum) * gdFontGetLarge()->w / 2), 10, datum,  white);
					
				}

				pngy = 600 - (pnga/10) * 10;
				gdImageSetPixel(im, pngx, pngy, realred);
				pngy = 600 - (sma_get()) * 10;
				gdImageSetPixel(im, pngx, 600-60*10, blue);
				gdImageSetPixel(im, pngx, 600-50*10, blue);
				gdImageSetPixel(im, pngx, 600-40*10, blue);
				gdImageSetPixel(im, pngx, 600-30*10, blue);
				gdImageSetPixel(im, pngx, 600-20*10, blue);
				gdImageSetPixel(im, pngx, 600-10*10, blue);
				gdImageSetPixel(im, pngx, 600-0*10, blue);
				gdImageLine(im, pngx, pngy, pngx, pngm, white2);
				gdImageSetPixel(im, pngx, pngy, white);
				
				
				pngx++;
				pngm = pngy;
				pnga = 0;
			}
			mid = mid + temp;
			if (middle < -273)
				middle = temp;
			else 
				middle = (middle + temp) / 2.0;
			if (temp > maximum)
				maximum = temp;
			if ((temp < minimum) || (minimum < -273.0))
				minimum = temp;
			//printf("<tr>\n");
			//printf("\t<td align='center'>%s</td><td align='center'>%s</td><td align='center'>%s°C</td><td>\n", datum, zeit, wert);
			
			if (temp < 0) {
				while(temp < 0) {
					//printf("|");
					temp = temp + 1.0;
				}
			} else {
				int i;
				temp *= temp;
				temp /= 10;
				i = 0;
				while(temp > 0) {
					//printf("|");
					i+=3;
					temp = temp - 1.0;
				}
				//printf("<span style=\"background-color:red; width:%ipx;\">&nbsp;</span>", i);
			}

			//printf("<td></tr>\n");
		}
	}
	printf("</table>\n");
	
	gdImageLine(im, 0, 0, 0, 600, white3);
	
	sprintf(buffer, "Messwerte: %i", count);
	gdImageString(im, gdFontGetLarge(), 20, 700, buffer,  white);
	sprintf(buffer, "Absolutes Maximum: %2.2lf°C", maximum);
	gdImageString(im, gdFontGetLarge(), 20, 650, buffer,  white);
	sprintf(buffer, "Absolutes Minimum: %2.2lf°C", minimum);
	gdImageString(im, gdFontGetLarge(), 20, 670, buffer,  white);
	sprintf(buffer, "Aktuelle Temperatur: %2.2lf°C (%s, %s)", current, lastdate, lasttime);
	gdImageString(im, gdFontGetLarge(), 20, 620, buffer,  white);
	
	sprintf(buffer, "SMA: %2.2lf°C", sma_get());
	gdImageString(im, gdFontGetLarge(), 520, 620, buffer,  white);
	


	pngout = fopen("/svr/httpd/htdocs/test.png", "wb");
	gdImagePng(im, pngout);
	fclose(pngout);

	gdImageDestroy(im);

	mid = mid / count;

	
	/*printf("<hr/><div><b>Absolutes Maximum:</b> %2.2lf</div>\n", maximum);
	printf("<div><b>Absolutes Minimum:</b> %2.2lf</div>\n", minimum);
	printf("<div><b>Messwerte:</b> %i</div>\n", count);
*/
	printf("<img src=\"/test.png\" name=\"bild\"/>");	
	fclose(fp);
	printf("<body>\n</html>\n");
}
