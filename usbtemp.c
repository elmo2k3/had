/***
 * Extended and made usable for project had by
 * 2009 Bjoern Biesenbach <bjoern@bjoern-b.de>
 *
 * Originally by:
 * USBtemp host control program
 * Copyright (C) 2008, 2009 Mathias Dalheimer (md@gonium.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Based on powerSwitch.c by Christian Starkjohann
 * of Objective Development Software GmbH (2005)
 * (see http://www.obdev.at/avrusb)
 * Some routines by Martin Thomas, see inline comments.
 */

/*
   General Description:
   This program queries the USBtemp hardware.
   It must be linked with libusb, a library for accessing the USB bus from
   Linux, FreeBSD, Mac OS X and other Unix operating systems. Libusb can be
   obtained from http://libusb.sourceforge.net/.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <usb.h>    /* this is libusb, see http://libusb.sourceforge.net/ */
#include "usbtemp.h"
#include "had.h"

/* Use obdev's generic shared VID/PID pair and follow the rules outlined
 * in firmware/usbdrv/USBID-License.txt.
 */

#define USB_SUCCESS		    0
#define USB_ERROR_NOTFOUND  1
#define USB_ERROR_ACCESS    2
#define USB_ERROR_IO        3
// Number of attempts to complete an USB operation before giving up.
#define USB_MAX_RETRIES		3

#define SENSOR_ID_UNKNOWN	  0
#define SENSOR_QUERY_OK		  1
#define SENSOR_QUERY_FAILURE  0 

/* These are the vendor specific SETUP commands implemented by our USB device */
#define USBTEMP_CMD_TEST			0
#define USBTEMP_CMD_NO_SENSORS		1
#define USBTEMP_CMD_QUERY_SENSOR    2
#define USBTEMP_CMD_GET_TEMP	    3

/* DS18X20 specific values (see datasheet) */
#define DS18S20_ID 0x10
#define DS18B20_ID 0x28
#define DS18X20_FRACCONV 625// constant to convert the fraction bits to cel*(10^-4)
#define DS18X20_ID_LENGTH 6 // The hardware ID is 6 bytes long.

static unsigned char usbOpenDevice(usb_dev_handle **device, int vendor, char *vendorName, int product, char *productName);
static int getTemperatureByName(usb_dev_handle *handle, char *sensor_name, double *sensor_value);

static int  usbGetStringAscii(usb_dev_handle *dev, int index, int langid, char *buf, int buflen)
{
  char    buffer[256];
  int     rval, i;

  if((rval = usb_control_msg(dev, USB_ENDPOINT_IN, USB_REQ_GET_DESCRIPTOR, (USB_DT_STRING << 8) + index, langid, buffer, sizeof(buffer), 1000)) < 0)
    return rval;
  if(buffer[1] != USB_DT_STRING)
    return 0;
  if((unsigned char)buffer[0] < rval)
    rval = (unsigned char)buffer[0];
  rval /= 2;
  /* lossy conversion to ISO Latin1 */
  for(i=1;i<rval;i++){
    if(i > buflen)  /* destination buffer overflow */
      break;
    buf[i-1] = buffer[2 * i];
    if(buffer[2 * i + 1] != 0)  /* outside of ISO Latin1 range */
      buf[i-1] = '?';
  }
  buf[i-1] = 0;
  return i-1;
}

static unsigned char usbOpenDevice(usb_dev_handle **device, int vendor, char *vendorName, int product, char *productName)
{
  struct usb_bus      *bus;
  struct usb_device   *dev;
  usb_dev_handle      *handle = NULL;
  unsigned char       errorCode = USB_ERROR_NOTFOUND;
  static int          didUsbInit = 0;

  if(!didUsbInit){
    didUsbInit = 1;
    usb_init();
  }
  usb_find_busses();
  usb_find_devices();
  for(bus=usb_get_busses(); bus; bus=bus->next){
    for(dev=bus->devices; dev; dev=dev->next){
      if(dev->descriptor.idVendor == vendor && dev->descriptor.idProduct == product){
        char    string[256];
        int     len;
        handle = usb_open(dev); /* we need to open the device in order to query strings */
        if(!handle){
          errorCode = USB_ERROR_ACCESS;
          continue;
        }
        if(vendorName == NULL && productName == NULL){  /* name does not matter */
          break;
        }
        /* now check whether the names match: */
        len = usbGetStringAscii(handle, dev->descriptor.iManufacturer, 0x0409, string, sizeof(string));
        if(len < 0){
          errorCode = USB_ERROR_IO;
        }else{
          errorCode = USB_ERROR_NOTFOUND;
          /* fprintf(stderr, "seen device from vendor ->%s<-\n", string); */
          if(strcmp(string, vendorName) == 0){
            len = usbGetStringAscii(handle, dev->descriptor.iProduct, 0x0409, string, sizeof(string));
            if(len < 0){
              errorCode = USB_ERROR_IO;
            }else{
              errorCode = USB_ERROR_NOTFOUND;
              /* fprintf(stderr, "seen product ->%s<-\n", string); */
              if(strcmp(string, productName) == 0)
                break;
            }
          }
        }
        usb_close(handle);
        handle = NULL;
      }
    }
    if(handle)
      break;
  }
  if(handle != NULL){
    errorCode = USB_SUCCESS;
    *device = handle;
  }
  return errorCode;
}

/* Developed by Martin Thomas, DS18X20 demo */
void printhex_nibble(const unsigned char b) {
  unsigned char  c = b & 0x0f;
  if (c>9) c += 'A'-10;
  else c += '0';
} 

/* Developed by Martin Thomas, DS18X20 demo */
void printhex_byte(const unsigned char  b) {
  printhex_nibble(b>>4);
  printhex_nibble(b);
} 

/***
 * retrieve the number of attached sensors.
 * TODO: Change error behaviour.
 */
unsigned char getNumSensors(usb_dev_handle *handle, unsigned char* amount) {
  unsigned char       buffer[8];
  unsigned char		  retval = SENSOR_QUERY_OK; // !=0 if an error occured.
  int                 nBytes;
  nBytes = usb_control_msg(handle, 
      USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, 
      USBTEMP_CMD_NO_SENSORS,		  // Command ID
      0,							  // Value 
      0,							  // Index 
      (char *) buffer, 
      sizeof(buffer), 
      5000);
  if(nBytes < 1){
	if(nBytes < 0) {
	}
	return USB_ERROR_IO;
  }
  *amount=buffer[0];
  return retval;
}

int getSensorById(usb_dev_handle *handle, char* sensor_name, int* sensorId) {
  unsigned char       buffer[8];
  unsigned char		  retval = USB_SUCCESS; // !=0 if an error occured.
  int                 nBytes;
  unsigned char amount=0;
  if (getNumSensors(handle, &amount)) {
  unsigned char found=1;
  *sensorId=SENSOR_ID_UNKNOWN; // make sure the value is defined.
  int i;
  for (i=0; i<amount; i++) { 
	nBytes = usb_control_msg(handle, 
		USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, 
		USBTEMP_CMD_QUERY_SENSOR,	  // Command ID
		i,							  // Value 
		0,							  // Index 
		(char *) buffer, 
		sizeof(buffer), 
		5000);
	if(nBytes < 1+DS18X20_ID_LENGTH){
	  if(nBytes < 0) {
		retval=USB_ERROR_IO;
		return retval;
	  }
	}
	found=1;
	int j;
	for(j=0; j<DS18X20_ID_LENGTH; j++) {
	  char id_part = buffer[j+1];
	  // Convert to string - nibble-wise hex representation.
	  unsigned char lsn = id_part & 0x0f;
	  if (lsn>9) lsn += 'A'-10;
	  else lsn += '0';
	  unsigned char msn = (id_part>>4) & 0x0f;
	  if (msn>9) msn += 'A'-10;
	  else msn += '0';
	  // compare this byte to the given ID represenation
	  if (! (msn == sensor_name[2*j] && lsn == sensor_name[2*j+1])) {
		found = 0;
		break;
	  }
	}
	if (found) {
	  *sensorId=i;
	  break;
	}
  }
  return retval;
  } else {
	return SENSOR_QUERY_FAILURE;
  }
}

int printTemperatureById(usb_dev_handle *handle, int sensorId) {
  unsigned char       buffer[8];
  unsigned char		  retval = SENSOR_QUERY_OK; // !=0 if an error occured.
  int                 nBytes;
  fprintf(stderr, "using sensor handle %u\n", sensorId);
  nBytes = usb_control_msg(handle, 
	  USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, 
	  USBTEMP_CMD_GET_TEMP,		  // Command ID
	  sensorId,					  // sensor id 
	  0,							  // Index 
	  (char *) buffer, 
	  sizeof(buffer), 
	  5000);
  if(nBytes < 4){
	if(nBytes < 0) {
	  fprintf(stderr, "USB error: %s\n", usb_strerror());
	}
	fprintf(stderr, "only %d bytes status received\n", nBytes);
	return SENSOR_QUERY_FAILURE;
  }
  unsigned char sensor = buffer[0];
  unsigned char subzero, cel, cel_frac_bits;
  subzero=buffer[1];
  cel=buffer[2];
  cel_frac_bits=buffer[3];
  fprintf(stderr, "reading sensor %d (°C): ", sensor);
  if (subzero)
	printf("-");
  else 
	printf("+");
  fprintf(stdout, "%d.", cel);
  fprintf(stdout, "%04d\n", cel_frac_bits*DS18X20_FRACCONV);
  return retval;
}

/**
 * read the temperature of the sensor with the given name. The name is
 * the ID of the sensor.
 */
int printTemperatureByName(usb_dev_handle *handle, char* sensor_name) {
  int sensorId=0;
  if (getSensorById(handle, sensor_name, &sensorId)) {
	fprintf(stderr, "Error: sensor not found.\n");
	return SENSOR_ID_UNKNOWN;
  }
  return printTemperatureById(handle, sensorId);
}

static int getTemperatureByName(usb_dev_handle *handle, char *sensor_name, double *sensor_value)
{
  unsigned char       buffer[8];
  unsigned char		  retval = SENSOR_QUERY_OK; // !=0 if an error occured.
  int                 nBytes;
  int sensorId;
  getSensorById(handle, sensor_name, &sensorId);
  nBytes = usb_control_msg(handle, 
	  USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, 
	  USBTEMP_CMD_GET_TEMP,		  // Command ID
	  sensorId,					  // sensor id 
	  0,							  // Index 
	  (char *) buffer, 
	  sizeof(buffer), 
	  5000);
  if(nBytes < 4){
	if(nBytes < 0) {
	}
	return SENSOR_QUERY_FAILURE;
  }
  unsigned char sensor = buffer[0];
  unsigned char subzero, cel, cel_frac_bits;
  subzero=buffer[1];
  cel=buffer[2];
  cel_frac_bits=buffer[3];
  if (subzero)
	  *sensor_value = -(double)cel+((double)cel_frac_bits*DS18X20_FRACCONV/10000);
  else 
	  *sensor_value = (double)cel+((double)cel_frac_bits*DS18X20_FRACCONV/10000);
  return retval;
}


int printSensors(usb_dev_handle *handle) {
  unsigned char       buffer[8];
  int                 nBytes;
  unsigned char amount=0;
  printf("Checking number of sensors\n");
  if (getNumSensors(handle, &amount)) {
	fprintf(stderr, "%d sensor(s) found, querying\n", amount);
	int i;
	for (i=0; i<amount; i++) { 
	  nBytes = usb_control_msg(handle, 
		  USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, 
		  USBTEMP_CMD_QUERY_SENSOR,	  // Command ID
		  i,							  // Value 
		  0,							  // Index 
		  (char *) buffer, 
		  sizeof(buffer), 
		  5000);
	  if(nBytes < 2){
		if(nBytes < 0) {
		  fprintf(stderr, "USB error: %s\n", usb_strerror());
		}
		fprintf(stderr, "only %d bytes status received\n", nBytes);
		return SENSOR_QUERY_FAILURE;
	  }
	  // TODO: Clean up, use struct to get clean response!
	  // buffer[0]: Type of sensor
	  // buffer[1-7]: 6-byte hardware id of sensor.
	  fprintf(stderr, "sensor %d: ID ", i);
	  int j;
	  for(j=1; j<7; j++) {
		char id_part = buffer[j];
		printhex_byte(id_part);
	  }
	  fprintf(stderr, " type: ");
	  if (buffer[0] == DS18S20_ID)
		fprintf(stderr, "(DS18S20)\n");
	  else if (buffer[0] == DS18B20_ID)
		fprintf(stderr, "(DS18B20)\n");
	}
	return SENSOR_QUERY_OK;
  } else {
	return SENSOR_QUERY_FAILURE;
  }
}

void usbTempLoop()
{
	int error, success;
	int error_count = 0;
	double temperature;
	int database_status;
	struct tm *ptm;
	struct tm time_saved;
	int decicelsius;
	time_t rawtime;
	int counter;
	usb_dev_handle      *handle = NULL;
  	usb_init();
	while(1)
	{
		if(!usbOpenDevice(&handle, USBDEV_SHARED_VENDOR, "gonium.net", USBDEV_SHARED_PRODUCT, "usbtemp"))
		{
			error_count = 0;
			for(counter = 0; counter < config.usbtemp_num_devices; counter++)
			{
				if(getTemperatureByName(handle, config.usbtemp_device_id[counter], &temperature))
				{
					time(&rawtime);
					memcpy(&time_saved, gmtime(&rawtime), sizeof(struct tm));
					decicelsius = abs((double)(temperature - (int)temperature)*10000.0);
					databaseInsertTemperature(config.usbtemp_device_modul[counter],
						config.usbtemp_device_sensor[counter],
						(int)temperature,decicelsius,&time_saved);
				}
			}
			usb_close(handle);
			sleep(30);
		}
		else
		{
			if(!error_count)
			{
				verbose_printf(2,"Could not connect to usbtemp device\n");
				error_count = 1;
			}
			sleep(1);
		}
	}
}

