/* (c) 2002-2005 by Marcin Wiacek and Michal Cihar */

#include <gammu-config.h>

#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifdef WIN32
#  include <io.h>
#  include <fcntl.h>
#  include <winsock2.h>
#else
#  include <netdb.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#endif

#include <gammu-keys.h>
#include <gammu-debug.h>

#include "../misc/coding/coding.h"
#include "../misc/misc.h"
#include "../gsmcomon.h"
#include "gsmmisc.h"

struct keys_table_position {
	char 	whatchar;
	int 	whatcode;
};

static struct keys_table_position Keys[] = {
	{'m',GSM_KEY_MENU},		{'M',GSM_KEY_MENU},
	{'n',GSM_KEY_NAMES},		{'N',GSM_KEY_NAMES},
	{'p',GSM_KEY_POWER},		{'P',GSM_KEY_POWER},
	{'u',GSM_KEY_UP},		{'U',GSM_KEY_UP},
	{'d',GSM_KEY_DOWN},		{'D',GSM_KEY_DOWN},
	{'+',GSM_KEY_INCREASEVOLUME},	{'-',GSM_KEY_DECREASEVOLUME},
	{'1',GSM_KEY_1},		{'2',GSM_KEY_2},	{'3',GSM_KEY_3},
	{'4',GSM_KEY_4},		{'5',GSM_KEY_5},	{'6',GSM_KEY_6},
	{'7',GSM_KEY_7},		{'8',GSM_KEY_8},	{'9',GSM_KEY_9},
	{'*',GSM_KEY_ASTERISK},		{'0',GSM_KEY_0},	{'#',GSM_KEY_HASH},
	{'g',GSM_KEY_GREEN},		{'G',GSM_KEY_GREEN},
	{'r',GSM_KEY_RED},		{'R',GSM_KEY_RED},
	{'<',GSM_KEY_LEFT},		{'>',GSM_KEY_RIGHT},
	{'[',GSM_KEY_SOFT1},		{']',GSM_KEY_SOFT2},
	{'h',GSM_KEY_HEADSET},		{'H',GSM_KEY_HEADSET},
	{'c',GSM_KEY_CLEAR},		{'C',GSM_KEY_CLEAR},
	{'j',GSM_KEY_JOYSTICK},		{'J',GSM_KEY_JOYSTICK},
	{'f',GSM_KEY_CAMERA},		{'F',GSM_KEY_CAMERA},
	{'o',GSM_KEY_OPERATOR},		{'O',GSM_KEY_OPERATOR},
	{'m',GSM_KEY_MEDIA},		{'M',GSM_KEY_MEDIA},
	{'d',GSM_KEY_DESKTOP},		{'D',GSM_KEY_DESKTOP},
	{'@',GSM_KEY_RETURN},
	{' ',0}
};

GSM_Error MakeKeySequence(char *text, GSM_KeyCode *KeyCode, size_t *Length)
{
	int 		i,j;
	unsigned char 	key;

	for (i=0;i<(int)(strlen(text));i++) {
		key 	   = text[i];
		KeyCode[i] = GSM_KEY_NONE;
		j 	   = 0;
		while (Keys[j].whatchar!=' ') {
	        	if (Keys[j].whatchar==key) {
				KeyCode[i]=Keys[j].whatcode;
				break;
			}
			j++;
		}
		if (KeyCode[i] == GSM_KEY_NONE) {
			*Length = i;
			return ERR_NOTSUPPORTED;
		}
	}
	*Length = i;
	return ERR_NONE;
}

GSM_Error GSM_ReadFile(char *FileName, GSM_File *File)
{
	int 		i = 1000;
	FILE		*file;
	struct stat	fileinfo;

	if (FileName[0] == 0x00) return ERR_UNKNOWN;
	file = fopen(FileName,"rb");
	if (file == NULL) return ERR_CANTOPENFILE;

	free(File->Buffer);
	File->Buffer 	= NULL;
	File->Used 	= 0;
	while (i == 1000) {
		File->Buffer 	= realloc(File->Buffer,File->Used + 1000);
		i 		= fread(File->Buffer+File->Used,1,1000,file);
		File->Used 	= File->Used + i;
	}
	File->Buffer = realloc(File->Buffer,File->Used + 1);
	/* Make it 0 terminated, in case it is needed somewhere (we don't count this to length) */
	File->Buffer[File->Used] = 0;
	fclose(file);

	File->ModifiedEmpty = true;
	if (stat(FileName,&fileinfo) == 0) {
		File->ModifiedEmpty = false;
		dbgprintf("File info read correctly\n");
		/* st_mtime is time of last modification of file */
		Fill_GSM_DateTime(&File->Modified, fileinfo.st_mtime);
		File->Modified.Year = File->Modified.Year + 1900;
		dbgprintf("FileTime: %02i-%02i-%04i %02i:%02i:%02i\n",
			File->Modified.Day,File->Modified.Month,File->Modified.Year,
			File->Modified.Hour,File->Modified.Minute,File->Modified.Second);
	}

	return ERR_NONE;
}

static void GSM_JADFindLine(GSM_File File, char *Name, char *Value)
{
	unsigned char 	Line[2000];
	size_t		Pos = 0;

	Value[0] = 0;

	while (1) {
		if (MyGetLine(File.Buffer, &Pos, Line, File.Used, sizeof(Line), false) != ERR_NONE) break;
		if (strlen(Line) == 0) break;
		if (!strncmp(Line,Name,strlen(Name))) {
			Pos = strlen(Name);
			while (Line[Pos] == 0x20) Pos++;
			strcpy(Value,Line+Pos);
			return;
		}
	}
}

GSM_Error GSM_JADFindData(GSM_File File, char *Vendor, char *Name, char *JAR, char *Version, int *Size)
{
	char Size2[200];

	GSM_JADFindLine(File, "MIDlet-Vendor:", Vendor);
	if (Vendor[0] == 0x00) return ERR_FILENOTSUPPORTED;
	dbgprintf("Vendor: \"%s\"\n",Vendor);

	GSM_JADFindLine(File, "MIDlet-Name:", Name);
	if (Name[0] == 0x00) return ERR_FILENOTSUPPORTED;
	dbgprintf("Name: \"%s\"\n",Name);

	GSM_JADFindLine(File, "MIDlet-Jar-URL:", JAR);
	if (JAR[0] == 0x00) return ERR_FILENOTSUPPORTED;
	dbgprintf("JAR file URL: \"%s\"\n",JAR);

	GSM_JADFindLine(File, "MIDlet-Jar-Size:", Size2);
	*Size = -1;
	if (Size2[0] == 0x00) return ERR_FILENOTSUPPORTED;
	dbgprintf("JAR size: \"%s\"\n",Size2);
	(*Size) = atoi(Size2);

	GSM_JADFindLine(File, "MIDlet-Version:", Version);
	dbgprintf("Version: \"%s\"\n",Version);

	return ERR_NONE;
}

void GSM_IdentifyFileFormat(GSM_File *File)
{
	File->Type = GSM_File_Other;
	if (File->Used > 2) {
		if (memcmp(File->Buffer, "BM",2)==0) {
			File->Type = GSM_File_Image_BMP;
		} else if (memcmp(File->Buffer, "GIF",3)==0) {
			File->Type = GSM_File_Image_GIF;
		} else if (File->Buffer[0] == 0x00 && File->Buffer[1] == 0x00) {
			File->Type = GSM_File_Image_WBMP;
		} else if (memcmp(File->Buffer+1, "PNG",3)==0) {
			File->Type = GSM_File_Image_PNG;
		} else if (File->Buffer[0] == 0xFF && File->Buffer[1] == 0xD8) {
			File->Type = GSM_File_Image_JPG;
		} else if (memcmp(File->Buffer, "MThd",4)==0) {
			File->Type = GSM_File_Sound_MIDI;
		} else if (File->Buffer[0] == 0x00 && File->Buffer[1] == 0x02) {
			File->Type = GSM_File_Sound_NRT;
		}
	}
}

PRINTF_STYLE(4, 5)
GSM_Error VC_StoreLine(char *Buffer, const size_t buff_len, size_t *Pos, const char *format, ...)
{
        va_list 		argp;
	int 			result;

	va_start(argp, format);
	result = vsnprintf(Buffer + (*Pos), buff_len - *Pos - 1, format, argp);
	va_end(argp);

	*Pos += result;
	if (*Pos >= buff_len - 1) return ERR_MOREMEMORY;

	result = snprintf(Buffer + (*Pos), buff_len - *Pos - 1, "%c%c", 13, 10);

	*Pos += result;
	if (*Pos >= buff_len - 1) return ERR_MOREMEMORY;

	return ERR_NONE;
}

PRINTF_STYLE(4, 5)
GSM_Error VC_Store(char *Buffer, const size_t buff_len, size_t *Pos, const char *format, ...)
{
        va_list 		argp;
	int 			result;

	va_start(argp, format);
	result = vsnprintf(Buffer + (*Pos), buff_len - *Pos - 1, format, argp);
	va_end(argp);

	*Pos += result;
	if (*Pos >= buff_len - 1) return ERR_MOREMEMORY;

	return ERR_NONE;
}


GSM_Error VC_StoreDateTime(char *Buffer, const size_t buff_len, size_t *Pos, GSM_DateTime *Date, char *Start)
{
	GSM_Error error;

	if (Start != NULL) {
		error = VC_Store(Buffer, buff_len, Pos, "%s:", Start);
		if (error != ERR_NONE) return error;
	}
	error = VC_StoreLine(Buffer, buff_len, Pos,
		"%04d%02d%02dT%02d%02d%02d%s",
			Date->Year, Date->Month, Date->Day,
			Date->Hour, Date->Minute, Date->Second,
			Date->Timezone == 0 ? "Z" : "");
	return error;
}

GSM_Error VC_StoreDate(char *Buffer, const size_t buff_len, size_t *Pos, GSM_DateTime *Date, char *Start)
{
	GSM_Error error;

	if (Start != NULL) {
		error = VC_Store(Buffer, buff_len, Pos, "%s:", Start);
		if (error != ERR_NONE) return error;
	}
	error = VC_StoreLine(Buffer, buff_len, Pos,
		"%04d%02d%02d",
			Date->Year, Date->Month, Date->Day);
	return error;
}

bool ReadVCALDateTime(const char *Buffer, GSM_DateTime *dt)
{
	time_t timestamp;
	char year[5]="", month[3]="", day[3]="", hour[3]="", minute[3]="", second[3]="";

	memset(dt,0,sizeof(GSM_DateTime));

	if (strlen(Buffer) < 8) return false;

	strncpy(year, 	Buffer, 	4);
	strncpy(month, 	Buffer+4, 	2);
	strncpy(day, 	Buffer+6, 	2);
	dt->Year	= atoi(year);
	dt->Month	= atoi(month);
	dt->Day		= atoi(day);

	if (Buffer[8] == 'T') {
		if (strlen(Buffer + 9) < 6) return false;

		strncpy(hour, 	Buffer+9,	2);
		strncpy(minute, Buffer+11,	2);
		strncpy(second, Buffer+13,	2);
		dt->Hour	= atoi(hour);
		dt->Minute	= atoi(minute);
		dt->Second	= atoi(second);

		/**
		 * @todo Handle properly timezone information
		 */
		if (Buffer[15] == 'Z') dt->Timezone = 0; /* Z = ZULU = GMT */
	}

	if (!CheckTime(dt)) {
		dbgprintf("incorrect date %d-%d-%d %d:%d:%d\n",dt->Day,dt->Month,dt->Year,dt->Hour,dt->Minute,dt->Second);
		return false;
	}
	if (dt->Year!=0) {
		if (!CheckDate(dt)) {
			dbgprintf("incorrect date %d-%d-%d %d:%d:%d\n",dt->Day,dt->Month,dt->Year,dt->Hour,dt->Minute,dt->Second);
			return false;
		}
	}

	if (dt->Timezone != 0) {
		timestamp = Fill_Time_T(*dt) + dt->Timezone;
		Fill_GSM_DateTime(dt, timestamp);
		dt->Year +=1900;
	}

	return true;
}

bool ReadVCALInt(char *Buffer, char *Start, int *Value)
{
	unsigned char buff[200];

	*Value = 0;

	strcpy(buff,Start);
	strcat(buff,":");
	if (!strncmp(Buffer,buff,strlen(buff))) {
		int lstart = strlen(Start);
		int lvalue = strlen(Buffer)-(lstart+1);
		strncpy(buff,Buffer+lstart+1,lvalue);
		strncpy(buff+lvalue,"\0",1);
		if (sscanf(buff,"%i",Value)) {
			dbgprintf("ReadVCalInt is \"%i\"\n",*Value);
			return true;
		}
	}
	return false;
}


bool ReadVCALDate(char *Buffer, char *Start, GSM_DateTime *Date, bool *is_date_only)
{
	char fullstart[200];
	unsigned char datestring[200];

	if (!ReadVCALText(Buffer, Start, datestring, false)) {
		fullstart[0] = 0;
		strcat(fullstart, Start);
		strcat(fullstart, ";VALUE=DATE");
		if (!ReadVCALText(Buffer, fullstart, datestring, false)) {
			return false;
		}
		*is_date_only = true;
	}

	if (ReadVCALDateTime(DecodeUnicodeString(datestring), Date)) {
		dbgprintf("ReadVCALDateTime is \"%04d.%02d.%02d %02d:%02d:%02d\"\n",
			Date->Year, Date->Month, Date->Day,
			Date->Hour, Date->Minute, Date->Second);
		return true;
	}

	return false;
}


GSM_Error VC_StoreText(char *Buffer, const size_t buff_len, size_t *Pos, unsigned char *Text, char *Start, bool UTF8)
{
	char *buffer;
	size_t len;
	GSM_Error error;

	len = UnicodeLength(Text);

	if (len == 0) return ERR_NONE;

	/* Need to be big enough to store quoted printable */
	buffer = (char *)malloc(len * 8);
	if (buffer == NULL) return ERR_MOREMEMORY;

	if (UTF8) {
		EncodeUTF8(buffer, Text);
		error =  VC_StoreLine(Buffer, buff_len, Pos, "%s:%s", Start, buffer);
	} else {
		EncodeUTF8QuotedPrintable(buffer,Text);
		if (UnicodeLength(Text) == strlen(buffer)) {
			/* Text is plain ASCII */
			error =  VC_StoreLine(Buffer, buff_len, Pos, "%s:%s", Start, buffer);
		} else {
			error =  VC_StoreLine(Buffer, buff_len, Pos, "%s;CHARSET=UTF-8;ENCODING=QUOTED-PRINTABLE:%s", Start, buffer);
		}
	}

	free(buffer);
	return error;
}

unsigned char *VCALGetTextPart(unsigned char *Buff, int *pos)
{
	static unsigned char	tmp[1000];
	unsigned char		*start;

	start = Buff + *pos;
	while (Buff[*pos] != 0 || Buff[*pos + 1] != 0) {
		if (Buff[*pos] == 0 && Buff[*pos + 1] == ';') {
			Buff[*pos + 1] = 0;
			CopyUnicodeString(tmp, start);
			Buff[*pos + 1] = ';';
			*pos += 2;
			return tmp;
		}
		*pos += 2;
	}
	if (start == Buff || (start[0] == 0 && start[1] == 0)) return NULL;
	CopyUnicodeString(tmp, start);
	return tmp;
}

/**
 * We separate matching text (Start) to tokens and then try to find all
 * tokens in Buffer. We also accept tokens like PREF, CHARSET or ENCODING.
 *
 * When all tokens are matched we found matching line.
 */
bool ReadVCALText(char *Buffer, char *Start, unsigned char *Value, bool UTF8)
{
	char *line = NULL;
	char **tokens = NULL;
	char *charset = NULL;
	char *begin, *pos, *end, *end2;
	bool quoted_printable = false;
	size_t numtokens, token;
	size_t i, j, len;
	bool found;
	bool ret = false;

	/* Initialize output */
	Value[0] = 0x00;
	Value[1] = 0x00;

	/* Count number of tokens */
	len = strlen(Start);
	numtokens = 1;
	for (i = 0; i < len; i++) {
		if (Start[i] == ';') {
			numtokens++;
		}
	}

	/* Allocate memory */
	line = strdup(Start);
	if (line == NULL) {
		dbgprintf("Could not alloc!\n");
		goto fail;
	}
	tokens = (char **)malloc(sizeof(char *) * numtokens);
	if (tokens == NULL) {
		dbgprintf("Could not alloc!\n");
		goto fail;
	}

	/* Parse Start to vCard tokens (separated by ;) */
	token = 0;
	begin = line;
	for (i = 0; i < len; i++) {
		if (line[i] == ';') {
			tokens[token++] = begin;
			begin = line + i + 1;
			line[i] = 0;
		}
	}
	/* Store last token */
	tokens[token++] = begin;

	/* Compare first token, it must be in place */
	pos = Buffer;
	len = strlen(tokens[0]);
	if (strncmp(pos, tokens[0], len) != 0) {
		goto fail;
	}
	/* Advance position */
	pos += len;
	/* No need to check this token anymore */
	tokens[0][0] = 0;

	/* Check remaining tokens */
	while (*pos != ':') {
		if (*pos == ';') {
			pos++;
		} else {
			dbgprintf("Could not parse!\n");
			goto fail;
		}
		found = false;
		for (token = 0; token < numtokens; token++) {
			len = strlen(tokens[token]);
			/* Skip already matched tokens */
			if (len == 0) {
				continue;
			}
			if (strncmp(pos, tokens[token], len) == 0) {
				dbgprintf("Found %s\n", tokens[token]);
				/* Advance position */
				pos += len;
				/* We need to check one token less */
				tokens[token][0] = 0;
				found = true;
				break;
			}
		}
		if (!found) {
			if (strncmp(pos, "ENCODING=QUOTED-PRINTABLE", 25) == 0) {
				quoted_printable = true;
				/* Advance position */
				pos += 25;
				found = true;
			} else if (strncmp(pos, "CHARSET=", 8) == 0) {
				/* Advance position */
				pos += 8;
				/* Grab charset */
				end = strchr(pos, ':');
				end2 = strchr(pos, ';');
				if (end == NULL && end2 == NULL) {
					dbgprintf("Could not read charset!\n");
					goto fail;
				} else if (end == NULL) {
					end = end2;
				} else if (end2 != NULL && end2 < end) {
					end = end2;
				}
				/* We basically want strndup, but it is not portable */
				charset = strdup(pos);
				if (charset == NULL) {
					dbgprintf("Could not alloc!\n");
					goto fail;
				}
				charset[end - pos] = 0;

				pos = end;
				found = true;
			} else if (strncmp(pos, "TZID=", 5) == 0) {
				/* @todo: We ignore time zone for now */
				/* Advance position */
				pos += 5;
				/* Go behind value */
				end = strchr(pos, ':');
				end2 = strchr(pos, ';');
				if (end == NULL && end2 == NULL) {
					dbgprintf("Could not read timezone!\n");
					goto fail;
				} else if (end == NULL) {
					end = end2;
				} else if (end2 != NULL && end2 < end) {
					end = end2;
				}
				pos = end;
				found = true;
			} else if (strncmp(pos, "TYPE=PREF", 9) == 0) {
				/* We ignore pref token */
				pos += 9;
				found = true;
			} else if (strncmp(pos, "PREF", 4) == 0) {
				/* We ignore pref token */
				pos += 4;
				found = true;
			}
			if (!found) {
				dbgprintf("%s not found!\n", Start);
				goto fail;
			}
		}
	}
	/* Skip : */
	pos++;
	/* Length of rest */
	len = strlen(pos);

	/* Did we match all our tokens? */
	for (token = 0; token < numtokens; token++) {
		if (strlen(tokens[token]) > 0) {
			dbgprintf("All tokens did not match!\n");
			goto fail;
		}
	}

	/* Decode the text */
	if (charset == NULL) {
		if (quoted_printable) {
			if (UTF8) {
				DecodeUTF8QuotedPrintable(Value, pos, len);
			} else {
				DecodeISO88591QuotedPrintable(Value, pos, len);
			}
		} else {
			if (UTF8) {
				DecodeUTF8(Value, pos, len);
			} else {
				DecodeISO88591(Value, pos, len);
			}
		}
	} else {
		if (strcasecmp(charset, "UTF-8") == 0||
				strcasecmp(charset, "\"UTF-8\"") == 0
				) {
			if (quoted_printable) {
				DecodeUTF8QuotedPrintable(Value, pos, len);
			} else {
				DecodeUTF8(Value, pos, len);
			}
		} else if (strcasecmp(charset, "UTF-7") == 0||
				strcasecmp(charset, "\"UTF-7\"") == 0
				) {
			if (quoted_printable) {
				dbgprintf("Unsupported charset: %s\n", charset);
				goto fail;
			} else {
				DecodeUTF7(Value, pos, len);
			}
		} else {
			dbgprintf("Unsupported charset: %s\n", charset);
			goto fail;
		}
	}

	/* Postprocess escaped chars */
	len = UnicodeLength(Value);
	for (i = 0; i < len; i++) {
		if (Value[(2 * i)] == 0 && Value[(2 * i) + 1] == '\\') {
			j = i + 1;
			if (Value[(2 * j)] == 0 && (
						Value[(2 * j) + 1] == 'n' ||
						Value[(2 * j) + 1] == 'N')
						) {
				Value[(2 * i) + 1] = '\n';
			} else if (Value[(2 * j)] == 0 && Value[(2 * j) + 1] == '\\') {
				Value[(2 * i) + 1] = '\\';
			} else if (Value[(2 * j)] == 0 && Value[(2 * j) + 1] == ';') {
				Value[(2 * i) + 1] = ';';
			} else if (Value[(2 * j)] == 0 && Value[(2 * j) + 1] == ',') {
				Value[(2 * i) + 1] = ',';
			} else {
				/* We ignore unknown for now */
				continue;
			}
			/* Shift the string */
			memmove(Value + (2 * j), Value + (2 * j) + 2, 2 * (len + 1 - j));
			len--;
		}
	}

	ret = true;
	dbgprintf("ReadVCalText(%s) is \"%s\"\n", Start, DecodeUnicodeConsole(Value));
fail:
	free(line);
	free(tokens);
	free(charset);
	return ret;
}

void GSM_ClearBatteryCharge(GSM_BatteryCharge *bat)
{
    bat->BatteryPercent = -1;
    bat->ChargeState = 0;
    bat->BatteryType = 0;
    bat->BatteryVoltage = -1;
    bat->ChargeVoltage = -1;
    bat->ChargeCurrent = -1;
    bat->PhoneCurrent = -1;
    bat->BatteryTemperature = -1;
    bat->PhoneTemperature = -1;
    bat->BatteryCapacity = -1;
}

/* How should editor hadle tabs in this file? Add editor commands here.
 * vim: noexpandtab sw=8 ts=8 sts=8:
 */
