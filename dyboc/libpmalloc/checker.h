#include <stdio.h>
#include <limits.h>
#define INT_BITS 32
#define UINT_BITS 32

#define __INT_CHECK__

/* Checker function definitions */

//Divison (/)
int SignedDivCheck(int si0, int si1, char* srcFName, unsigned int srcLine);
unsigned int UnsignedDivheck(unsigned int ui0, unsigned int ui1);

//Reminder (%)
int SignedRemCheck(int si0, int si1, char* srcFName, unsigned int srcLine);
unsigned int UnsignedRemCheck(unsigned int ui0, unsigned int ui1, char* srcFName, unsigned int srcLine);

//Addition (+)
int SignedAddCheck(int si0, int si1, char* srcFName, unsigned int srcLine);
unsigned int UnsignedAddCheck(unsigned int ui0, unsigned int ui1, char* srcFName, unsigned int srcLine);

//Substraction (-)
int SignedSubCheck(int si0, int si1, char* srcFName, unsigned int srcLine);
unsigned int UnsignedSubCheck(unsigned int ui0, unsigned int ui1, char* srcFName, unsigned int srcLine);

//Multiplication (*)
int SignedMulCheck(int si0, int si1, char* srcFName, unsigned int srcLine);
unsigned int UnsignedMulCheck(unsigned int ui0, unsigned int ui1, char* srcFName, unsigned int srcLine);

//Shift right (>>)
int SignedShrCheck(int si0, int si1, char* srcFName, unsigned int srcLine);
unsigned int UnsignedShrCheck(unsigned int ui0, unsigned int ui1, char* srcFName, unsigned int srcLine);

//Shift left (<<)
int SignedShlCheck(int si0, int si1, char* srcFName, unsigned int srcLine);
unsigned int UnSignedShlCheck(unsigned int ui0, unsigned int ui1, char* srcFName, unsigned int srcLine);

//Unary increment
int SignedPreInc(int* si, char* srcFName, unsigned int srcLine);
unsigned UnsignedPreInc (unsigned int* ui, char* srcFName, unsigned int srcLine);

int SignedPostInc(int* si, char* srcFName, unsigned int srcLine);
unsigned UnsignedPostInc (unsigned int* ui, char* srcFName, unsigned int srcLine);

//Unary decrement
int SignedPreDec(int* si, char* srcFName, unsigned int srcLine);
unsigned UnsignedPreDec (unsigned int* ui, char* srcFName, unsigned int srcLine);

int SignedPostDec(int* si, char* srcFName, unsigned int srcLine);
unsigned UnsignedPostDec (unsigned int* ui, char* srcFName, unsigned int srcLine);
