#ifndef CMDLN_HPP
#define CMDLN_HPP

bool cmdln_save(int argc, char **argv);

int cmdln_patch(const char *orig_name, const char *new_name, char ***argvp);

#endif
