#ifndef OPER_H
#define OPER_H

/* like scp l_fn srv:r_fn */
int add_cp_to_server(const char *srv, const char *l_fn, const char *r_fn);
/* like scp srv:r_fn l_fn */
int add_cp_from_server(const char *srv, const char *l_fn, const char *r_fn);
/* like ssh -T srv cmd */
int add_exec(const char *srv, const char *cmd);
/* start execute */
int oper_exec();

#endif
