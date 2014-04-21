#include "checker.h"

/*
 * Implementations
 */

static void LOG (char* checker, char* file, unsigned int line) {
   fprintf (stderr, "Error detected: %s from %s (line %u) \n", checker, file, line);
}

int SignedDivCheck(int si0, int si1, char* srcFName, unsigned int srcLine) {
#ifdef  __INT_CHECK__
   if ((si0 == INT_MAX && si1 == -1) ||  (si1 == 0)){
      //need action here
      LOG("SignedDivCheck", srcFName, srcLine);
   }
#endif

   return si0 / si1;
}

unsigned int UnsignedDivCheck(unsigned int ui0, unsigned int ui1, char* srcFName, unsigned int srcLine) {
#ifdef __INT_CHECK__
   if (ui1 == 0) {
      //need action here
      LOG("UnsignedDivCheck", srcFName, srcLine);
   }
#endif

   return ui0 % ui1;
}

int SignedRemCheck(int si0, int si1, char* srcFName, unsigned int srcLine) {
#ifdef  __INT_CHECK__
   if ((si0 == INT_MAX && si1 == -1) ||  (si1 == 0)){
      //need action here
      LOG("SignedRemCheck", srcFName, srcLine);
   }
#endif

   return si0 / si1;
}
unsigned int UnsignedRemCheck(unsigned int ui0, unsigned int ui1, char* srcFName, unsigned int srcLine) {
#ifdef __INT_CHECK__
   if (ui1 == 0) {
      //need action here
      LOG("UnsignedRemCheck", srcFName, srcLine);
   }
#endif

   return ui0 % ui1;
}

// (((si1>0) && (si2>0) && (si1 > (INT_MAX-si2)))
// || ((si1<0) && (si2<0) && (si1 < (INT_MIN-si2))))
int SignedAddCheck(int si0, int si1, char* srcFName, unsigned int srcLine) {
#ifdef __INT_CHECK__
   if ( ((si0 > 0) && (si1 > 0) && (si0 > (INT_MAX - si1))) 
        || ((si0 < 0) && (si1 < 0) && (si0 < (INT_MIN - si1)))
      ) {
      //need an action here
      LOG("SignedAddCheck", srcFName, srcLine);
   }
#endif

   return si0 + si1;
}

// ui1 > UINT_MAX - ui0
unsigned int UnsignedAddCheck(unsigned int ui0, unsigned int ui1, char* srcFName, unsigned int srcLine) {
#ifdef __INT_CHECK__
   if (ui1 > UINT_MAX - ui0 ) {
      //need an action here
      LOG("UnsignedAddCheck", srcFName, srcLine);
   }
#endif

   return ui0 + ui1;
}

// si1 < 0 ? si0 > INT_MAX + si1 : si0 < INT_MIN + si1
int SignedSubCheck(int si0, int si1, char* srcFName, unsigned int srcLine){
#ifdef __INT_CHECK__
   if ( si1 < 0 ? si0 > INT_MAX + si1 : si0 < INT_MIN + si1) {
      //need an action here
      LOG("SignedSubCheck", srcFName, srcLine);
   }
#endif

   return si0 - si1;
}

unsigned int UnsignedSubCheck(unsigned int ui0, unsigned int ui1, char* srcFName, unsigned int srcLine) {
#ifdef __INT_CHECK__
   if (ui0 < ui1) {
      //need an action here
      LOG("UnsignedSubCheck", srcFName, srcLine);
   }
#endif

   return ui0 - ui1;
}


// (((si0 > 0) && (si1 > 0) && (si0 > (INT_MAX / si1)))
// || ((si0 > 0) && (si1 < 0) && (si1 < (INT_MIN / si0)))
// || ((si0 < 0) && (si1 > 0) && (si0 < (INT_MIN / si1)))
// || ((si0 < 0) && (si1 < 0) && (si0 != 0) && (si1 < (INT_MAX / si0))))
int SignedMulCheck(int si0, int si1, char* srcFName, unsigned int srcLine) {
#ifdef __INT_CHECK__
   if (((si0 > 0) && (si1 > 0) && (si0 > (INT_MAX / si1)))
     || ((si0 > 0) && (si1 < 0) && (si1 < (INT_MIN / si0)))
     || ((si0 < 0) && (si1 > 0) && (si0 < (INT_MIN / si1)))
     || ((si0 < 0) && (si1 < 0) && (si0 != 0) && (si1 < (INT_MAX / si0)))) 
   {
      //need an action here
      LOG("SignedMulCheck", srcFName, srcLine);
   }
#endif

   return si0 * si1;
}

unsigned int UnsignedMulCheck(unsigned int ui0, unsigned int ui1, char* srcFName, unsigned int srcLine) {
#ifdef __INT_CHECK__
   if ( ui0 > (UINT_MAX / ui1 ) ) {
      //need an action here
      LOG("UnsignedMulCheck", srcFName, srcLine);
   }
#endif

   return ui0 * ui1;
}

int SignedShrCheck(int si0, int si1, char* srcFName, unsigned int srcLine){
#ifdef __INT_CHECK__
   if ((si1 < 0) || (si1 >= INT_BITS)) {
      //need an action here
      LOG("SignedShrCheck", srcFName, srcLine);
   }
#endif

   return si0 >> si1;
}

unsigned int UnsignedShrCheck(unsigned int ui0, unsigned int ui1, char* srcFName, unsigned int srcLine) {
#ifdef __INT_CHECK__
   if (ui1 >= UINT_BITS) {
      //need an action here
      LOG("UnsignedShrCheck", srcFName, srcLine);
   }
#endif

   return ui0 >> ui1;
}

int SignedShlCheck(int si0, int si1, char* srcFName, unsigned int srcLine) {
#ifdef __INT_CHECK__
   if ((si1 < 0) || (si1 >= INT_BITS)) {
      //need an action here
      LOG("SignedShlCheck", srcFName, srcLine);
   }
#endif

   return si0 << si1;
}

unsigned int UnsignedShlCheck(unsigned int ui0, unsigned int ui1, char* srcFName, unsigned int srcLine) {
#ifdef __INT_CHECK__
   if (ui1 >= UINT_BITS) {
      //need an action here
      LOG("UnsignedShlCheck", srcFName, srcLine);
   }
#endif

   return ui0 << ui1;
}

//Increment
int SignedPreInc(int* si, char* srcFName, unsigned int srcLine) {
#ifdef __INT_CHECK__

#endif
   *si++;
   return *si - 1;
}

unsigned UnsignedPreInc (unsigned int* ui, char* srcFName, unsigned int srcLine) {
#ifdef __INT_CHECK__

#endif
   *ui++;
   return *ui - 1;
}

int SignedPostInc(int* si, char* srcFName, unsigned int srcLine) {
#ifdef __INT_CHECK__

#endif
   *si++;
   return *si;
}

unsigned UnsignedPostInc (unsigned int* ui, char* srcFName, unsigned int srcLine) {
#ifdef __INT_CHECK__

#endif
   *ui++;
   return *ui;
}

//Decrement
int SignedPreDec(int* si, char* srcFName, unsigned int srcLine) {
#ifdef __INT_CHECK__

#endif
   *si--;
   return *si - 1;
}

unsigned UnsignedPreDec (unsigned int* ui, char* srcFName, unsigned int srcLine) {
#ifdef __INT_CHECK__

#endif
   *ui--;
   return *ui - 1;
}

int SignedPostDec(int* si, char* srcFName, unsigned int srcLine) {
#ifdef __INT_CHECK__

#endif
   *si--;
   return *si;
}

unsigned UnsignedPostDec (unsigned int* ui, char* srcFName, unsigned int srcLine) {
#ifdef __INT_CHECK__

#endif
   *ui--;
   return *ui;
}
