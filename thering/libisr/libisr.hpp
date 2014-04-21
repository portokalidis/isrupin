#ifndef LIBISR_HPP
#define LIBISR_HPP

int libisr_init(const char *dbfn);

void libisr_cleanup();

bool libisr_known_image(ADDRINT addr);

bool libisr_code_injection(INT32 sig, const CONTEXT *ctx, 
		const EXCEPTION_INFO *pExceptInfo);

#endif
