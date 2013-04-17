/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"
/* Find out about weak transactions.
 *  User A does a transaction.
 *  User B does somethign without a transaction, and it conflicts.
 */


#include <db.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/stat.h>

static void
test_autotxn (u_int32_t env_flags, u_int32_t db_flags) {
    DB_ENV *env;
    DB *db;
    int r;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    r = db_env_create (&env, 0);           CKERR(r);
    env->set_errfile(env, stderr);
    r = env->set_flags(env, env_flags, 1); CKERR(r);
    r = env->open(env, ENVDIR, 
                  DB_CREATE | DB_PRIVATE | DB_INIT_MPOOL | 
                  DB_INIT_LOG | DB_INIT_TXN | DB_INIT_LOCK, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = db_create(&db, env, 0);
    CKERR(r);
    {
	DB_TXN *x = NULL;
	if (env_flags==0 && db_flags==0) {
	    r = env->txn_begin(env, 0, &x, 0); CKERR(r);
	}
	r = db->open(db, x, "numbers.db", 0, DB_BTREE, DB_CREATE | db_flags, 0);
	if (env_flags==0 && db_flags==0) {
	    r = x->commit(x, 0); CKERR(r);
	}
	CKERR(r);
    }

    DB_TXN *x1, *x2 = NULL;
    r = env->txn_begin(env, 0, &x1, DB_TXN_NOWAIT); CKERR(r);
    #ifdef USE_BDB
        r = env->txn_begin(env, 0, &x2, DB_TXN_NOWAIT); CKERR(r);
    #endif
    DBT k1,k2,v1,v2;
    dbt_init(&k1, "hello", sizeof "hello");
    dbt_init(&k2, "hello", sizeof "hello");
    dbt_init(&v1, "there", sizeof "there");
    dbt_init(&v2, NULL, 0);
    memset(&v1, 0, sizeof(DBT));
    memset(&v2, 0, sizeof(DBT));
    r = db->put(db, x1, &k1, &v1, 0); CKERR(r);
    r = db->get(db, x2, &k2, &v2, 0); assert(r==DB_LOCK_DEADLOCK || r==DB_LOCK_NOTGRANTED);
    r = x1->commit(x1, 0);         CKERR(r);
    #ifdef USE_BDB
        r = x2->commit(x2, 0);     assert(r==0);
    #endif
    r = db->close(db, 0);          CKERR(r);
    r = env->close(env, 0);        assert(r==0);
}

int
test_main (int argc __attribute__((__unused__)), char *const argv[] __attribute__((__unused__)))  {
    test_autotxn(DB_AUTO_COMMIT, DB_AUTO_COMMIT); 
    test_autotxn(0,              DB_AUTO_COMMIT); 
    test_autotxn(DB_AUTO_COMMIT, 0); 
    test_autotxn(0,              0); 
    return 0;
}