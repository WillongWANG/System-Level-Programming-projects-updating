#include <stdlib.h>
#include <string.h>
#include "debugmalloc.h"
#include "dmhelper.h"
#include <stdio.h>
#define FENCE 0xCCDEADCC

typedef struct {
	int checksum;
	char* filename;
	int linenumber;
	int size; // the size of payload
	int fence;
} header;

typedef struct {
	int fence; // fence indicating whether writing past footer
} footer;

typedef struct node {
	header* h;
	struct node* next;
} node;

static node header_node = { NULL, NULL };//以node为结点的链表的首结点（h=NULL）

/* Wrappers for malloc and free */

void* MyMalloc(size_t size, char* filename, int linenumber) {
	header h; footer f; char* temp = (char*)&h; char* t = (char*)&f; char* t1, * t2;
	h.checksum = 0; h.filename = filename; h.linenumber = linenumber; h.size = size; h.fence = FENCE;
	f.fence = FENCE;
	int s = sizeof(header); int i, c = 0;
	node* ptmp = &header_node; node fn;
	void* temp1 = malloc(sizeof(header) + size + sizeof(footer)), *temp2=malloc(sizeof(node));

	while (s--) {
		// for the char the num of bits is 8
		for (i = 0; i < 8; i++) {
			c += (*temp >> i) & 0x1;         //temp是一地址（字节编址），指向一字节，最后的c是h中为1的bit的个数
		}
		temp++;
	}
	h.checksum = c; s = sizeof(header);
	temp = (char*)&h;

	t1 = (char*)temp1;

	while (s--)
	{
		*t1 = *temp;
		temp++; t1 = t1 + 1;
	}

	s = sizeof(footer); t1 = (char*)temp1 + sizeof(header) + size;
	while (s--)
	{
		*t1 = *t;
		t++; t1 = t1 + 1;
	}

	fn.h = (header*)temp1; fn.next = NULL; 
	s = sizeof(node); t1 = (char*)temp2; t2 = (char*)&fn;
	while (s--)
	{
		*t1 = *t2;
		t2++; t1 = t1 + 1;
	}

	while (ptmp->next) {
		ptmp = ptmp->next;
	}

	ptmp->next = (node*)temp2;

	return (void*)((char*)temp1 + sizeof(header));
}

void MyFree(void* ptr, char* filename, int linenumber) {
	header* h = (header*)((char*)ptr - sizeof(header)); header* hh;
	int a, c = 0, i, fl = 0; char* t = (char*)h; int s = sizeof(header); node* ptmp = &header_node;

	//error4、5
	while (ptmp->next)
	{
		ptmp = ptmp->next;
		hh = ptmp->h;
		if (hh == h) { fl = 1; break; }
	}
	if (fl == 0) { error(4, filename, linenumber); return; }

	a = h->checksum; h->checksum = 0;
	while (s--) {
		// for the char the num of bits is 8
		for (i = 0; i < 8; i++) {
			c += (*t >> i) & 0x1;
		}
		t++;
	}
	if (c != a)   {error(3, filename, linenumber); return;}
	//可能h->filename, h->linenumber，h->size都不正确了
/*这个就不必要了，因为fence不对则checksum肯定不对；checksum若不用fence则更好
  if (h->fence != FENCE) {
		errorfl(1, h->filename, h->linenumber, filename, linenumber);
	} */   

	footer* f = (footer*)((char*)ptr + h->size);
	if (f->fence != FENCE) {
		errorfl(2, h->filename, h->linenumber, filename, linenumber);
	}


	//从node链表中删除这一节点
	ptmp = &header_node; void* p;
	while (ptmp->next)
	{
		hh = ptmp->next->h;
		if (hh == h) { p = ptmp->next; ptmp->next = ptmp->next->next; break; }
		ptmp = ptmp->next;
	}
	
	free((char*)ptr - sizeof(header)); free(p);

}



/* Goes through the currently allocated blocks and checks
	to see if they are all valid.
	Returns -1 if it receives an error, 0 if all blocks are
	okay.
应先运行HeapCheck(),再运行下面两个函数，否则header的size可能被篡改是错的*/
int HeapCheck() {
	node* ptmp = &header_node; footer* f; header* h; int a, c = 0, i = 0; char* t; int s = sizeof(header);
	while (ptmp->next) {
		ptmp = ptmp->next;
		h = ptmp->h;
		t = (char*)h;

		a = h->checksum; h->checksum = 0;
		while (s--) {
			// for the char the num of bits is 8
			for (i = 0; i < 8; i++) {
				c += (*t >> i) & 0x1;
			}
			t++;
		}
		if (c != a) { PRINTERROR(3, h->filename, h->linenumber); return -1; }//可能h->filename, h->linenumber，h->size都不正确了

		f = (footer*)((char*)h + sizeof(header) + h->size);
		if (f->fence != FENCE) {
			PRINTERROR(2, h->filename, h->linenumber); return -1;
		}

	}

	return 0;
}


/* returns number of bytes allocated using MyMalloc/MyFree:
	used as a debugging tool to test for memory leaks */
int AllocatedSize() {
	node* ptmp = &header_node; header* hh; int i = 0;
	while (ptmp->next) {
		ptmp = ptmp->next;
		hh = ptmp->h;
		i += hh->size;
	}
	return i;
}


/* Optional functions */

/* Prints a list of all allocated blocks with the
	filename/line number when they were MALLOC'd */
void PrintAllocatedBlocks() {
	node* ptmp = &header_node; header* hh;
	while (ptmp->next) {
		ptmp = ptmp->next;
		hh = ptmp->h;
		PRINTBLOCK(hh->size, hh->filename, hh->linenumber);
	}
	return;
}

