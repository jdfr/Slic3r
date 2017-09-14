#ifdef SLIC3RXS
#include <xsinit.h>
#else
#include "libslic3r.h"
#include <stdio.h>
#endif
#include "utils.hpp" 
#include <regex>

FILE *STDERRCONFESS=stderr;

void setConfessErrOutput(FILE *err) {
  STDERRCONFESS=err;
}

void
confess_at(const char *file, int line, const char *func,
            const char *pat, ...)
{
    #ifdef SLIC3RXS
     va_list args;
     SV *error_sv = newSVpvf("Error in function %s at %s:%d: ", func,
         file, line);

     va_start(args, pat);
     sv_vcatpvf(error_sv, pat, &args);
     va_end(args);

     sv_catpvn(error_sv, "\n\t", 2);

     dSP;
     ENTER;
     SAVETMPS;
     PUSHMARK(SP);
     XPUSHs( sv_2mortal(error_sv) );
     PUTBACK;
     call_pv("Carp::confess", G_DISCARD);
     FREETMPS;
     LEAVE;
    #else
      va_list args;
      fprintf(STDERRCONFESS, "Error in function %s at %s:%d: ", func, file, line);
      vfprintf(STDERRCONFESS, pat, args);
      fprintf(STDERRCONFESS, "\n");
    #endif
}

std::vector<std::string> 
split_at_regex(const std::string& input, const std::string& regex) {
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
        first{input.begin(), input.end(), re, -1},
        last;
    return {first, last};
}
