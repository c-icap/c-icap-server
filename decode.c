/*
 *  Copyright (C) 2004-2008 Christos Tsantilas
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301  USA.
 */


#include "common.h"
#include "c-icap.h"
#include "simple_api.h"

unsigned char base64_table[] = {
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,  62, 255, 255, 255,  63,
      52,  53,  54,  55,  56,  57,  58,  59,  60,  61, 255, 255, 255,   0, 255, 255,
     255,   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
      15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25, 255, 255, 255, 255, 255,
     255,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,
      41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255
};


int ci_base64_decode(char *encoded, char *decoded, int len)
{
    int i;
    unsigned char *str,*result;
    
    if (!encoded || !decoded || !len)
	return 0;

    str = (unsigned char *)encoded;
    result = (unsigned char *)decoded;
    
    for (i=len; i>3; i-=3) {
	
	/*if one of the last 4 bytes going to be proccessed is not valid just 
	  stops processing. This "if" cover the '\0' string termination character
	  of str (because base64_table[0]=255)
	 */
	if(base64_table[*str]>63 || base64_table[*(str+1)] > 63 || 
	   base64_table[*(str+2)] > 63 ||base64_table[*(str+3)] > 63)
	    break;
	
	/*6 bits from the first + 2 last bits from second*/
	*(result++)=(base64_table[*str] << 2) | (base64_table[*(str+1)] >>4);
	/*last 4 bits from second + first 4 bits from third*/
	*(result++)=(base64_table[*(str+1)] << 4) | (base64_table[*(str+2)] >>2);
	/*last 2 bits from third + 6 bits from forth */
	*(result++)=(base64_table[*(str+2)] << 6) | (base64_table[*(str+3)]);
	str += 4;
    }
    *result='\0';
    return len-i;
}


char *ci_base64_decode_dup(char *encoded) 
{
    int len;
    char *result;
    len=strlen(encoded);
    len=((len+3)/4)*3+1;
    if(!(result=malloc(len*sizeof(char))))
	return NULL;
   
    ci_base64_decode(encoded,result,len);
    return result;
}


/*url decoders  */
int url_decoder(char *input,char *output, int output_len)
{
    int i, k;
    char str[3];
    
    i = 0;
    k = 0;
    while ((input[i] != '\0') && (k < output_len-1)) {
	if (input[i] == '%'){ 
	    str[0] = input[i+1];
	    str[1] = input[i+2];
	    str[2] = '\0';
	    output[k] = strtol(str, NULL, 16);
	    i = i + 3;
	}
	else if (input[i] == '+') {
	    output[k] = ' ';
	    i++;
	}
	else {
	    output[k] = input[i];
	    i++;
	}
	k++;
	}
    output[k] = '\0';

    if (k == output_len-1)
	return -1;

    return 1;
}

int url_decoder2(char *input)
{
    int i, k;
    char str[3];    
    i = 0;
    k = 0;
    while (input[i] != '\0') {
	if (input[i] == '%') {
	    str[0] = input[i+1];
	    str[1] = input[i+2];
	    str[2] = '\0';
	    input[k] = strtol(str, NULL, 16);
	    i = i + 3;
	}
	else if (input[i] == '+') {
	    input[k] = ' ';
	    i++;
	}
	else {
	    input[k] = input[i];
	    i++;
	}
	k++;
    }
    input[k] = '\0';
    return 1;
}
