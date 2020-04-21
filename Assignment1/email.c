#include "postgres.h"

#include "regex.h"
#include "string.h"

#include "fmgr.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

PG_MODULE_MAGIC;

typedef int int4;

typedef struct EmailAddr
{
	int4 length_var;
	u_int8_t local_length;
	char addr[0];
}EmailAddr;


PG_FUNCTION_INFO_V1(emailaddr_in);

Datum
emailaddr_in(PG_FUNCTION_ARGS)
{
	char* str = PG_GETARG_CSTRING(0);
	char* tempLocal;
	char* tempDomain;
	EmailAddr* result;
	regex_t regex;
    	int reti;
	u_int8_t localL;
	u_int8_t domainL;
	int i;

    regcomp(&regex, "^[a-z]([a-z0-9-]*[a-z0-9])?(\\.[a-z]([a-z0-9-]*[a-z0-9])?)*@[a-z]([a-z0-9-]*[a-z0-9])?\\.[a-z]([a-z0-9-]*[a-z0-9])?(\\.[a-z]([a-z0-9-]*[a-z0-9])?)*$", REG_ICASE | REG_EXTENDED);

    reti = regexec(&regex, str, 0, NULL, 0);
    regfree(&regex);
    if (reti) {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for emailaddr: \"%s\"", str)));
    }

	tempLocal = strtok(str, "@");
	tempDomain = strtok(NULL, "@");

	localL = strlen(tempLocal);
	domainL = strlen(tempDomain);

	result = (EmailAddr* )palloc(VARHDRSZ + sizeof(u_int8_t) + (localL + domainL + 2) * sizeof(char));

	strncpy(&(result->addr[0]), tempLocal, localL);
	result->addr[localL] = '\0';
	strncpy(&(result->addr[localL + 1]), tempDomain, domainL);
	result->addr[localL + domainL + 1] = '\0';

	for(i = 0; i <= localL + domainL + 1; ++i) {
		if(result->addr[i] >= 65 && result->addr[i] <= 90) {
			result->addr[i] += 32;
		}
	}

	result->local_length = localL;

	SET_VARSIZE(result, VARHDRSZ + sizeof(u_int8_t) + (localL + domainL + 2) * sizeof(char));

	PG_RETURN_POINTER(result);
}


PG_FUNCTION_INFO_V1(emailaddr_out);

Datum
emailaddr_out(PG_FUNCTION_ARGS)
{
	EmailAddr* emailaddr = (EmailAddr* )PG_GETARG_POINTER(0);
	char* result;

	result = psprintf("%s@%s", &(emailaddr->addr[0]), &(emailaddr->addr[emailaddr->local_length + 1]));

	PG_RETURN_CSTRING(result);
}


static int
emailaddr_cmp_internal(EmailAddr* a, EmailAddr* b)
{
	if ((strcasecmp(&(a->addr[a->local_length + 1]), &(b->addr[b->local_length + 1]))) > 0) {
		return 1;
	}
	if ((strcasecmp(&(a->addr[a->local_length + 1]), &(b->addr[b->local_length + 1]))) == 0)  {
		if((strcasecmp(&(a->addr[0]), &(b->addr[0]))) > 0 ){
			return 1;
		}
		if((strcasecmp(&(a->addr[0]), &(b->addr[0]))) < 0 ){
			return -1;
		}
		return 0;
	}
	return -1;
}


PG_FUNCTION_INFO_V1(emailaddr_lt);

Datum
emailaddr_lt(PG_FUNCTION_ARGS)
{
	EmailAddr* a = (EmailAddr* ) PG_GETARG_POINTER(0);
	EmailAddr* b = (EmailAddr* ) PG_GETARG_POINTER(1);
	PG_RETURN_BOOL(emailaddr_cmp_internal(a, b) < 0);
}


PG_FUNCTION_INFO_V1(emailaddr_le);

Datum
emailaddr_le(PG_FUNCTION_ARGS)
{
	EmailAddr* a = (EmailAddr* ) PG_GETARG_POINTER(0);
	EmailAddr* b = (EmailAddr* ) PG_GETARG_POINTER(1);
	PG_RETURN_BOOL(emailaddr_cmp_internal(a, b) <= 0);
}


PG_FUNCTION_INFO_V1(emailaddr_eq);

Datum
emailaddr_eq(PG_FUNCTION_ARGS)
{
	EmailAddr* a = (EmailAddr* ) PG_GETARG_POINTER(0);
	EmailAddr* b = (EmailAddr* ) PG_GETARG_POINTER(1);
	PG_RETURN_BOOL(emailaddr_cmp_internal(a, b) == 0);
}


PG_FUNCTION_INFO_V1(emailaddr_ne);

Datum
emailaddr_ne(PG_FUNCTION_ARGS)
{
	EmailAddr* a = (EmailAddr* ) PG_GETARG_POINTER(0);
	EmailAddr* b = (EmailAddr* ) PG_GETARG_POINTER(1);
	PG_RETURN_BOOL(emailaddr_cmp_internal(a, b) != 0);
}


PG_FUNCTION_INFO_V1(emailaddr_ge);

Datum
emailaddr_ge(PG_FUNCTION_ARGS)
{
	EmailAddr* a = (EmailAddr* ) PG_GETARG_POINTER(0);
	EmailAddr* b = (EmailAddr* ) PG_GETARG_POINTER(1);
	PG_RETURN_BOOL(emailaddr_cmp_internal(a, b) >= 0);
}


PG_FUNCTION_INFO_V1(emailaddr_gt);

Datum
emailaddr_gt(PG_FUNCTION_ARGS)
{
	EmailAddr* a = (EmailAddr* ) PG_GETARG_POINTER(0);
	EmailAddr* b = (EmailAddr* ) PG_GETARG_POINTER(1);
	PG_RETURN_BOOL(emailaddr_cmp_internal(a, b) > 0);
}


PG_FUNCTION_INFO_V1(emailaddr_cmp);

Datum
emailaddr_cmp(PG_FUNCTION_ARGS)
{
	EmailAddr* a = (EmailAddr* ) PG_GETARG_POINTER(0);
	EmailAddr* b = (EmailAddr* ) PG_GETARG_POINTER(1);

	PG_RETURN_INT32(emailaddr_cmp_internal(a, b));
}


static int
emailaddr_domain_cmp_internal(EmailAddr* a, EmailAddr* b){
	if ((strcasecmp(&(a->addr[a->local_length + 1]), &(b->addr[b->local_length + 1]))) == 0){
		return 1;
	}else{
		return 0;
	}
}


PG_FUNCTION_INFO_V1(emailaddr_domain_eq);

Datum
emailaddr_domain_eq(PG_FUNCTION_ARGS){
	EmailAddr* a =(EmailAddr* )PG_GETARG_POINTER(0); 
	EmailAddr* b = (EmailAddr* ) PG_GETARG_POINTER(1);
	PG_RETURN_BOOL(emailaddr_domain_cmp_internal(a, b) == 1);
}


PG_FUNCTION_INFO_V1(emailaddr_domain_ne);

Datum
emailaddr_domain_ne(PG_FUNCTION_ARGS){
	EmailAddr* a =(EmailAddr* )PG_GETARG_POINTER(0); 
	EmailAddr* b = (EmailAddr* ) PG_GETARG_POINTER(1);
	PG_RETURN_BOOL(emailaddr_domain_cmp_internal(a, b) == 0);
}


PG_FUNCTION_INFO_V1(emailaddr_domain_cmp);

Datum
emailaddr_domain_cmp(PG_FUNCTION_ARGS){
	EmailAddr* a =(EmailAddr* )PG_GETARG_POINTER(0); 
	EmailAddr* b = (EmailAddr* ) PG_GETARG_POINTER(1);
	PG_RETURN_INT32(emailaddr_domain_cmp_internal(a, b));
}


PG_FUNCTION_INFO_V1(emailaddr_abs_hash);

Datum
emailaddr_abs_hash(PG_FUNCTION_ARGS)
{
    EmailAddr* emailaddr = (EmailAddr* ) PG_GETARG_POINTER(0);
    char* address;
    Datum result;
    
    int len = strlen(&(emailaddr->addr[0])) + strlen(&(emailaddr->addr[emailaddr->local_length + 1])) + 1 + 1;
    
    address = (char* ) palloc(sizeof(char) * len);
    
    snprintf(address, sizeof(char) * len, "%s@%s",  &(emailaddr->addr[0]), &(emailaddr->addr[emailaddr->local_length + 1]));
    
    result = hash_any((unsigned char* ) address, strlen(address));
    
    PG_RETURN_DATUM(result);
}
