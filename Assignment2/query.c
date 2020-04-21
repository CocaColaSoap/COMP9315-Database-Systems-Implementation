// query.c ... query scan functions
// part of Multi-attribute Linear-hashed Files
// Manage creating and using Query objects
// Last modified by John Shepherd, July 2019

#include "defs.h"
#include "query.h"
#include "reln.h"
#include "tuple.h"
#include "hash.h"
#include "page.h"
// A suggestion ... you can change however you like

struct QueryRep {
	Reln    rel;       // need to remember Relation info
	char    *processedhashval;     // the hash value from MAH
	char 	*querystr;     // the hash value from MAH
	PageID  curpage;   // current page in scan
	int     is_ovflow; // are we in the overflow pages?
	Offset  curtup;    // offset of current tuple within page
	//TODO
};

// take a query string (e.g. "1234,?,abc,?")
// set up a QueryRep object for the scan

int checkQuery(Reln r, char *q){
	if(*q == '\0') return 0;
	char *c;
	int count = 1;
	for(c = q; *c != '\0';c++){
		if(*c == ',')
			count++;
	}
	return count==nattrs(r);
}

unsigned int bitwise_get(Bits src, int src_bit) {
	return ((src >> (sizeof(Bits) * 8 - 1 - src_bit)) & 1);
}

Query startQuery(Reln r, char *q)
{
	Query new = malloc(sizeof(struct QueryRep));
	assert(new != NULL);
	Count numofatt = nattrs(r);
	if(checkQuery(r,q)==0)
		return NULL;
	ChVecItem *cv = chvec(r);
	char **vals = malloc(numofatt*sizeof(char *));
	Bits hashval[numofatt];
	char* xx = calloc(MAXBITS, sizeof(char));
	tupleVals(q,vals);
	for(int i=0;i< numofatt;i++){
		if(strcmp(vals[i],"?")==0){
			for(int j=0;j < MAXBITS;j++){
				if(cv[j].att==i){
					xx[MAXBITS-j-1] = 9;
				}
			}
		}
		else{
			hashval[i] = hash_any((unsigned char *)vals[i],strlen(vals[i]));
			for(int k=0;k < MAXBITS;k++){
				if(cv[k].att==i){
					xx[MAXBITS-k-1] = bitwise_get(hashval[i],MAXBITS-cv[k].bit-1);
				}
			}
		}
	}
	new -> rel = r;
	new -> is_ovflow = -1;
	new -> processedhashval = xx;
	new -> curpage = 0;
	new -> curtup = 0;
	new -> querystr = q;
	return new;

	// TODO
	// Partial algorithm:
	// form known bits from known attributes
	// form unknown bits from '?' attributes
	// compute PageID of first page
	//   using known bits and first "unknown" value
	// set all values in QueryRep object
}

// get next tuple during a scan

// void bitwise_char(Bits src,char *pid_bits){
// 	printf("%d ",src);
// 	int len = (sizeof(pid_bits)/sizeof(char));
// 	for(int kk = 0; kk < len; kk++) {
// 		if (src & (1 << kk)) {
// 			pid_bits[kk] = 1;
// 		} else {
// 			pid_bits[kk] = 0;
// 		}
// 	}
// }

void bitwise_char(Bits src,char *pid_bits){
	for(int kk = sizeof(unsigned int) * 8 - 1; kk >= 0; --kk){
		pid_bits[MAXBITS-kk-1] = src >> kk & 1;
	}
}

Tuple getNextTuple(Query q)
{
	char pid_bits[MAXBITS];
	char *prohashval=q->processedhashval;
	Reln r = q->rel;
	// printf("\n");	
	//printf("%s",q->querystr);
	for(int pid=q->curpage;pid < npages(r);pid++){
		bitwise_char(pid,pid_bits);
		Bool flag = TRUE;
		if(pid < splitp(r)){
			for(int j = MAXBITS-depth(r)-1;j<MAXBITS;j++){
				if(prohashval[j] == 9){
					continue;
				}
				else if(pid_bits[j] != prohashval[j]){
					flag = FALSE;
				break;
				}
			}
		}
		else{
			for(int j = MAXBITS-depth(r);j<MAXBITS;j++){
				if(prohashval[j] == 9){
					continue;
				}
				else if(pid_bits[j] != prohashval[j]){
					flag = FALSE;
				break;
				}
			}
		}
		
		//printf("%hhd",flag);
		if(flag == FALSE){
			continue;
		}
		else{
			//printf("%d ",pid);
			Count hdr_size = 2*sizeof(Offset) + sizeof(Count);
			int dataSize = PAGESIZE - hdr_size;
			if(q->is_ovflow == -1){
				int count = q->curtup;
				Page p = getPage(dataFile(r),pid);
				char *data = pageData(p);
				for(int h=q->curtup;h < dataSize;h++){
					if(data[h] == '\0'){
						if(h == count){
							break;
						}
						if(tupleMatch(r,&data[count],q->querystr) == TRUE){
							q->curpage = pid;
							q->curtup = h+1;
							free(p);
							return &data[count];
						}
						else{
								count = h+1;
							}
					}
				}
				if(pageOvflow(p)!= -1){
					q->is_ovflow = pageOvflow(p);
					q->curtup = 0;
				}
				else{
					q->curpage = pid+1;
					q->curtup = 0;
				}
				free(p);
			}
			if(q->is_ovflow != -1){
				while(q->is_ovflow != -1){
					//printf(" overflow:%d ",q->is_ovflow);
					int count = q->curtup;
					Page p = getPage(ovflowFile(r), q->is_ovflow);
					char *data = pageData(p);
					for(int h=q->curtup;h < dataSize;h++){
						if(data[h] == '\0'){
							if(h == count){
								break;
							}
							if(tupleMatch(r,&data[count],q->querystr) == TRUE){
								q->curtup = h+1;
								free(p);
								return &data[count];
							}
							else{
								count = h+1;
							}
						}
					}
					if(pageOvflow(p)!= -1){
						q->is_ovflow = pageOvflow(p);
						q->curtup = 0;
					}else{
						q->is_ovflow = -1;
						q->curtup = 0;
						q->curpage = pid+1;
					}
					free(p);
				}
			}
		}
	}
	
		
	// for(int i = new->curpage;i < npages(r);i++){
	// 	if()
	// }

	// TODO
	// Partial algorithm:
	// if (more tuples in current page)
	//    get next matching tuple from current page
	// else if (current page has overflow)
	//    move to overflow page
	//    grab first matching tuple from page
	// else
	//    move to "next" bucket
	//    grab first matching tuple from data page
	// endif
	// if (current page has no matching tuples)
	//    go to next page (try again)
	// endif
	return NULL;
}

// clean up a QueryRep object and associated data

void closeQuery(Query q)
{
	free(q);
	// TODO
}
