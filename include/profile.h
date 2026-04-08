#ifndef PROFILE_H
#define PROFILE_H

#include "tscrt.h"

#ifdef __cplusplus
extern "C" {
#endif

int  profile_init(profile_t *p);
int  profile_load(profile_t *p);
int  profile_save(const profile_t *p);
int  profile_add_session(profile_t *p, const session_entry_t *s);
int  profile_delete_session(profile_t *p, int index);

#ifdef __cplusplus
}
#endif

#endif /* PROFILE_H */
