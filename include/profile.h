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

/* Import/export helpers: like profile_load/profile_save but accept an
 * explicit file path instead of using p->profile_path. profile_load_from
 * does not fall back to seeding a sample profile when the file is
 * missing — it returns -1 instead. */
int  profile_load_from(profile_t *p, const char *path);
int  profile_save_to(const profile_t *p, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* PROFILE_H */
