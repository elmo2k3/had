#ifndef __HR20_H__
#define __HR20_H__

struct _hr20info
{
	int16_t tempis;
	int16_t tempset;
	int8_t valve;
	int16_t voltage;
	int8_t mode;
};

extern int hr20GetStatus(struct _hr20info *hr20info);

#endif

