/*
 * Copyright (c) 2016 Mindaugas Rasiukevicius <rmind at noxt eu>
 * All rights reserved.
 *
 * Use is subject to license terms, as specified in the LICENSE file.
 */

#ifndef	_EBR_H_
#define	_EBR_H_

__BEGIN_DECLS

struct ebr;
typedef struct ebr ebr_t;

#define	EBR_EPOCHS	3

ebr_t *		ebr_create(void);
void		ebr_destroy(ebr_t *);
int		ebr_register(ebr_t *);
void		ebr_unregister(ebr_t *);

void		ebr_enter(ebr_t *);
void		ebr_exit(ebr_t *);
bool		ebr_sync(ebr_t *, unsigned *);
unsigned	ebr_staging_epoch(ebr_t *);
unsigned	ebr_gc_epoch(ebr_t *);
void		ebr_full_sync(ebr_t *, unsigned);
bool		ebr_incrit_p(ebr_t *);

__END_DECLS

#endif
