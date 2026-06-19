#include "../include/cube.h"

void itoa(char *s, int i) { sprintf_s(s)("%d", i); };
char *exchangestr(char *o, char *n) {
  gp()->deallocstr(o);
  return newstring(n);
};

/// Contains ALL vars/commands/aliases
hashtable<ident> *idents = NULL;

void alias(char *name, char *action) {
  ident *b = idents->access(name);
  if (!b) {
    name = newstring(name);
    ident b = {ID_ALIAS, name, 0, 0, 0, 0, 0, newstring(action), true, NULL};
    idents->access(name, &b);
  } else {
    if (b->type == ID_ALIAS)
      b->action = exchangestr(b->action, action);
    else
      conoutf("Cannot redefine builtin %s with an alias", name);
  };
};

COMMAND(alias, ARG_2STR);
static int __ad_alias =
    (addcommanddetail("alias", "Creates or modifies an alias"), 0);

/// Variables and commands are registered through globals, see cube.h
int variable(char *name, int min, int cur, int max, int *storage, void (*fun)(),
             bool persist) {
  if (!idents)
    idents = new hashtable<ident>;
  ident v = {ID_VAR, name, min, max, storage, fun, 0, 0, persist, NULL};
  idents->access(name, &v);
  return cur;
};

void setvar(char *name, int i) { *idents->access(name)->storage = i; };
int getvar(char *name) { return *idents->access(name)->storage; };
bool identexists(char *name) { return idents->access(name) != NULL; };

char *getalias(char *name) {
  ident *i = idents->access(name);
  return i && i->type == ID_ALIAS ? i->action : NULL;
};

bool addcommand(char *name, void (*fun)(), int narg) {
  if (!idents)
    idents = new hashtable<ident>;
  ident c = {ID_COMMAND, name, 0, 0, 0, fun, narg, 0, false, NULL};
  idents->access(name, &c);
  return false;
};

void addcommanddetail(const char *name, const char *detail) {
  if (!idents)
    return;
  ident *id = idents->access((char *)name);
  if (id) {
    if (id->detail)
      gp()->deallocstr(id->detail);
    id->detail = newstring(detail);
  }
}

const char *getargsig(const ident *id) {
  static string buf;
  if (id->type == ID_VAR) {
    if (id->min > id->max) {
      sprintf_s(buf)("(int) read-only");
    } else {
      sprintf_s(buf)("(int) %d..%d", id->min, id->max);
    }
    return buf;
  }
  if (id->type == ID_ALIAS) {
    buf[0] = 0;
    return buf;
  }
  switch (id->narg) {
  case ARG_1INT:
    strcpy_s(buf, "(int)");
    break;
  case ARG_2INT:
    strcpy_s(buf, "(int, int)");
    break;
  case ARG_3INT:
    strcpy_s(buf, "(int, int, int)");
    break;
  case ARG_4INT:
    strcpy_s(buf, "(int, int, int, int)");
    break;
  case ARG_NONE:
    buf[0] = 0;
    break;
  case ARG_1STR:
    strcpy_s(buf, "(str)");
    break;
  case ARG_2STR:
    strcpy_s(buf, "(str, str)");
    break;
  case ARG_3STR:
    strcpy_s(buf, "(str, str, str)");
    break;
  case ARG_5STR:
    strcpy_s(buf, "(str, str, str, str, str)");
    break;
  case ARG_DOWN:
    strcpy_s(buf, "(key)");
    break;
  case ARG_DWN1:
    strcpy_s(buf, "(key, str)");
    break;
  case ARG_1EXP:
    strcpy_s(buf, "(exp)");
    break;
  case ARG_2EXP:
    strcpy_s(buf, "(exp, exp)");
    break;
  case ARG_1EST:
    strcpy_s(buf, "(str)");
    break;
  case ARG_2EST:
    strcpy_s(buf, "(str, str)");
    break;
  case ARG_VARI:
    strcpy_s(buf, "(...)");
    break;
  default:
    buf[0] = 0;
    break;
  }
  return buf;
}

const char *getargsig_byname(const char *name) {
  if (!idents)
    return NULL;
  ident *id = idents->access((char *)name);
  return id ? getargsig(id) : NULL;
}

const char *getdetail(const char *name) {
  if (!idents)
    return NULL;
  ident *id = idents->access((char *)name);
  return id ? id->detail : NULL;
}

char *parseexp(char *&p, int right) // parse any nested set of () or []
{
  int left = *p++;
  char *word = p;
  for (int brak = 1; brak;) {
    int c = *p++;
    if (c == '\r')
      *(p - 1) = ' ';
    if (c == left)
      brak++;
    else if (c == right)
      brak--;
    else if (!c) {
      p--;
      conoutf("missing \"%c\"", right);
      return NULL;
    };
  };
  char *s = newstring(word, p - word - 1);
  if (left == '(') {
    string t;
    itoa(t, execute(s)); // evaluate () exps directly, and substitute result
    s = exchangestr(s, t);
  };
  return s;
};

char *parseword(char *&p) // parse single argument, including expressions
{
  p += strspn(p, " \t\r");
  if (p[0] == '/' && p[1] == '/')
    p += strcspn(p, "\n\0");
  if (*p == '\"') {
    p++;
    char *word = p;
    p += strcspn(p, "\"\r\n\0");
    char *s = newstring(word, p - word);
    if (*p == '\"')
      p++;
    return s;
  };
  if (*p == '(')
    return parseexp(p, ')');
  if (*p == '[')
    return parseexp(p, ']');
  char *word = p;
  p += strcspn(p, "; \t\r\n\0");
  if (p - word == 0)
    return NULL;
  return newstring(word, p - word);
};

char *lookup(char *n) // find value of ident referenced with $ in exp
{
  ident *id = idents->access(n + 1);
  if (id)
    switch (id->type) {
    case ID_VAR:
      string t;
      itoa(t, *(id->storage));
      return exchangestr(n, t);
    case ID_ALIAS:
      return exchangestr(n, id->action);
    };
  conoutf("Unknown alias lookup: %s", n + 1);
  return n;
};

int execute(char *p, bool isdown) // all evaluation happens here, recursively
{
  const int MAXWORDS = 25; // limit, remove
  char *w[MAXWORDS];
  int val = 0;
  for (bool cont = true; cont;) // for each ; seperated statement
  {
    int numargs = MAXWORDS;
    loopi(MAXWORDS) // collect all argument values
    {
      w[i] = "";
      if (i > numargs)
        continue;
      char *s = parseword(p); // parse and evaluate exps
      if (!s) {
        numargs = i;
        s = "";
      };
      if (*s == '$')
        s = lookup(s); // substitute variables
      w[i] = s;
    };

    p += strcspn(p, ";\n\0");
    cont = *p++ != 0; // more statements if this isn't the end of the string
    char *c = w[0];
    if (*c == '/')
      c++; // strip irc-style command prefix
    if (!*c)
      continue; // empty statement

    ident *id = idents->access(c);
    if (!id) {
      val = ATOI(c);
      if (!val && *c != '0')
        conoutf("Unknown command: %s", c);
    } else
      switch (id->type) {
      case ID_COMMAND: {
        static int argcounts[] = {1, 2, 3, 4, 0, 1, 2, 3, 5, 0, 1, 1, 2, 1, 2};
        if (id->narg != ARG_VARI) {
          int want = argcounts[id->narg];
          if (numargs - 1 > want) {
            conoutf("The %s function accepts only %d argument(s)", c, want);
            break;
          }
        }
        switch (id->narg) {
        case ARG_1INT:
          if (isdown)
            ((void(__cdecl *)(int))id->fun)(ATOI(w[1]));
          break;
        case ARG_2INT:
          if (isdown)
            ((void(__cdecl *)(int, int))id->fun)(ATOI(w[1]), ATOI(w[2]));
          break;
        case ARG_3INT:
          if (isdown)
            ((void(__cdecl *)(int, int, int))id->fun)(ATOI(w[1]), ATOI(w[2]),
                                                      ATOI(w[3]));
          break;
        case ARG_4INT:
          if (isdown)
            ((void(__cdecl *)(int, int, int, int))id->fun)(
                ATOI(w[1]), ATOI(w[2]), ATOI(w[3]), ATOI(w[4]));
          break;
        case ARG_NONE:
          if (isdown)
            ((void(__cdecl *)())id->fun)();
          break;
        case ARG_1STR:
          if (isdown)
            ((void(__cdecl *)(char *))id->fun)(w[1]);
          break;
        case ARG_2STR:
          if (isdown)
            ((void(__cdecl *)(char *, char *))id->fun)(w[1], w[2]);
          break;
        case ARG_3STR:
          if (isdown)
            ((void(__cdecl *)(char *, char *, char *))id->fun)(w[1], w[2],
                                                               w[3]);
          break;
        case ARG_5STR:
          if (isdown)
            ((void(__cdecl *)(char *, char *, char *, char *, char *))id->fun)(
                w[1], w[2], w[3], w[4], w[5]);
          break;
        case ARG_DOWN:
          ((void(__cdecl *)(bool))id->fun)(isdown);
          break;
        case ARG_DWN1:
          ((void(__cdecl *)(bool, char *))id->fun)(isdown, w[1]);
          break;
        case ARG_1EXP:
          if (isdown)
            val = ((int(__cdecl *)(int))id->fun)(execute(w[1]));
          break;
        case ARG_2EXP:
          if (isdown)
            val = ((int(__cdecl *)(int, int))id->fun)(execute(w[1]),
                                                      execute(w[2]));
          break;
        case ARG_1EST:
          if (isdown)
            val = ((int(__cdecl *)(char *))id->fun)(w[1]);
          break;
        case ARG_2EST:
          if (isdown)
            val = ((int(__cdecl *)(char *, char *))id->fun)(w[1], w[2]);
          break;
        case ARG_VARI:
          if (isdown) {
            string r; // limit, remove
            r[0] = 0;
            for (int i = 1; i < numargs; i++) {
              strcat_s(r, w[i]); // make string-list out of all arguments
              if (i == numargs - 1)
                break;
              strcat_s(r, " ");
            };
            ((void(__cdecl *)(char *))id->fun)(r);
            break;
          }
        };
        break;
      }

      case ID_VAR:
        if (numargs > 2) {
          conoutf("The %s function accepts only 1 argument", c);
          break;
        }
        if (isdown) {
          if (!w[1][0])
            conoutf("%s = %d", c, *id->storage); // var with no value just
                                                 // prints its current value
          else {
            if (id->min > id->max) {
              conoutf("This variable is read-only");
            } else {
              int i1 = ATOI(w[1]);
              if (i1 < id->min || i1 > id->max) {
                i1 = i1 < id->min ? id->min : id->max; // clamp to valid range
                conoutf("Valid range for %s is %d..%d", c, id->min, id->max);
              }
              *id->storage = i1;
            };
            if (id->fun)
              ((void(
                  __cdecl *)())id->fun)(); // call trigger function if available
          };
        };
        break;

      case ID_ALIAS: // alias, also used as functions and (global) variables
        for (int i = 1; i < numargs; i++) {
          sprintf_sd(t)("arg%d", i); // set any arguments as (global) arg values
                                     // so functions can access them
          alias(t, w[i]);
        };
        char *action = newstring(id->action); // create new string here because
                                              // alias could rebind itself
        val = execute(action, isdown);
        gp()->deallocstr(action);
        break;
      };
    loopj(numargs) gp()->deallocstr(w[j]);
  };
  return val;
};

// tab-completion of all idents

int completesize = 0, completeidx = 0;

void resetcomplete() { completesize = 0; };

void complete(char *s) {
  if (*s != '/') {
    string t;
    strcpy_s(t, s);
    strcpy_s(s, "/");
    strcat_s(s, t);
  };
  if (!s[1])
    return;
  if (!completesize) {
    completesize = (int)strlen(s) - 1;
    completeidx = 0;
  };
  int idx = 0;
  enumerate(
      idents, ident *, id,
      if (strncmp(id->name, s + 1, completesize) == 0 && idx++ == completeidx) {
        strcpy_s(s, "/");
        strcat_s(s, id->name);
      };);
  completeidx++;
  if (completeidx >= idx)
    completeidx = 0;
};

bool execfile(const char *cfgfile) {
  string s;
  strcpy_s(s, cfgfile);
  char *buf = loadfile(path(s), NULL);
  if (!buf)
    return false;
  execute(buf);
  free(buf);
  return true;
};

void exec(const char *cfgfile) {
  if (!execfile(cfgfile))
    conoutf("Could not read \"%s\"", cfgfile);
};

void writecfg() {
  FILE *f = fopen("config.cfg.tmp", "w");
  if (!f)
    return;
  fprintf(
      f,
      "// This file contains all player settings for the game.\n"
      "// Delete this file to have data/default.cfg executed and written "
      "here.\n"
      "// If you want to modify settings below, do it after closing the game,\n"
      "// because they'll get overwritten without being read, otherwise.\n\n");

  writeclientinfo(f);
  fprintf(f, "\n");
  enumerate(
      idents, ident *, id, if (id->type == ID_VAR && id->persist) {
        fprintf(f, "%s %d\n", id->name, *id->storage);
      };);
  fprintf(f, "\n");
  writebinds(f);
  fprintf(f, "\n");
  enumerate(
      idents, ident *, id,
      if (id->type == ID_ALIAS && !strstr(id->name, "nextmap_")) {
        fprintf(f, "alias \"%s\" [%s]\n", id->name, id->action);
      };);
  fclose(f);
  rename("config.cfg.tmp", "config.cfg");
};

COMMAND(writecfg, ARG_NONE);
static int __ad_writecfg =
    (addcommanddetail("writecfg", "Saves current configuration to config.cfg"),
     0);

void dangerresetcfg() {
  remove("config.cfg");
  execfile("data/default.cfg");
  writecfg();
  conoutf("Config has been reset to defaults.");
};

COMMAND(dangerresetcfg, ARG_NONE);
static int __ad_resetcfg =
    (addcommanddetail("dangerresetcfg",
                      "Resets config.cfg to defaults, does not provide prompt"),
     0);

void intset(char *name, int v) {
  string b;
  itoa(b, v);
  alias(name, b);
};

void ifthen(char *cond, char *thenp, char *elsep) {
  execute(cond[0] != '0' ? thenp : elsep);
};

void loopa(char *times, char *body) {
  int t = atoi(times);
  loopi(t) {
    intset("i", i);
    execute(body);
  };
};

void whilea(char *cond, char *body) {
  while (execute(cond))
    execute(body);
};

void onrelease(bool on, char *body) {
  if (!on)
    execute(body);
};

void concat(char *s) { alias("s", s); };

void concatword(char *s) {
  for (char *a = s, *b = s; *a = *b; b++)
    if (*a != ' ')
      a++;
  concat(s);
};

int listlen(char *a) {
  if (!*a)
    return 0;
  int n = 0;
  while (*a)
    if (*a++ == ' ')
      n++;
  return n + 1;
};

void at(char *s, char *pos) {
  int n = atoi(pos);
  loopi(n) s += strspn(s += strcspn(s, " \0"), " ");
  s[strcspn(s, " \0")] = 0;
  concat(s);
};

COMMANDN(loop, loopa, ARG_2STR);
static int __ad_loop =
    (addcommanddetail("loop", "Loops body expression a given number of times"),
     0);
COMMANDN(while, whilea, ARG_2STR);
static int __ad_while =
    (addcommanddetail("while",
                      "Repeatedly executes body while condition is true"),
     0);
COMMANDN(if, ifthen, ARG_3STR);
static int __ad_if =
    (addcommanddetail("if", "Conditional execution with then/else branches"),
     0);
COMMAND(onrelease, ARG_DWN1);
static int __ad_onrelease =
    (addcommanddetail("onrelease", "Executes body when key is released"), 0);
COMMAND(exec, ARG_1STR);
static int __ad_exec = (addcommanddetail("exec", "Executes a script file"), 0);
COMMAND(concat, ARG_VARI);
static int __ad_concat =
    (addcommanddetail("concat", "Concatenates arguments into a single string"),
     0);
COMMAND(concatword, ARG_VARI);
static int __ad_concatword =
    (addcommanddetail("concatword", "Concatenates arguments removing spaces"),
     0);
COMMAND(at, ARG_2STR);
static int __ad_at =
    (addcommanddetail("at", "Gets the nth word from a string"), 0);
COMMAND(listlen, ARG_1EST);
static int __ad_listlen =
    (addcommanddetail("listlen", "Returns number of words in a string"), 0);

int add(int a, int b) { return a + b; };
COMMANDN(+, add, ARG_2EXP);
static int __ad_add = (addcommanddetail("+", "Adds two expressions"), 0);
int mul(int a, int b) { return a * b; };
COMMANDN(*, mul, ARG_2EXP);
static int __ad_mul = (addcommanddetail("*", "Multiplies two expressions"), 0);
int sub(int a, int b) { return a - b; };
COMMANDN(-, sub, ARG_2EXP);
static int __ad_sub = (addcommanddetail("-", "Subtracts two expressions"), 0);
int divi(int a, int b) { return b ? a / b : 0; };
COMMANDN(div, divi, ARG_2EXP);
static int __ad_div = (addcommanddetail("div", "Divides two expressions"), 0);
int mod(int a, int b) { return b ? a % b : 0; };
COMMAND(mod, ARG_2EXP);
static int __ad_mod = (addcommanddetail("mod", "Modulo of two expressions"), 0);
int equal(int a, int b) { return (int)(a == b); };
COMMANDN(=, equal, ARG_2EXP);
static int __ad_equal =
    (addcommanddetail("=", "Checks if two expressions are equal"), 0);
int lt(int a, int b) { return (int)(a < b); };
COMMANDN(<, lt, ARG_2EXP);
static int __ad_lt =
    (addcommanddetail("<", "Checks if first expression is less than second"),
     0);
int gt(int a, int b) { return (int)(a > b); };
COMMANDN(>, gt, ARG_2EXP);
static int __ad_gt =
    (addcommanddetail(">", "Checks if first expression is greater than second"),
     0);

int strcmpa(char *a, char *b) { return strcmp(a, b) == 0; };
COMMANDN(strcmp, strcmpa, ARG_2EST);
static int __ad_strcmp =
    (addcommanddetail("strcmp", "Compares two strings for equality"), 0);

int rndn(int a) { return a > 0 ? rnd(a) : 0; };
COMMANDN(rnd, rndn, ARG_1EXP);
static int __ad_rnd =
    (addcommanddetail("rnd",
                      "Returns random integer less than the given number"),
     0);

int explastmillis() { return lastmillis; };
COMMANDN(millis, explastmillis, ARG_1EXP);
static int __ad_millis =
    (addcommanddetail("millis", "Returns current time in milliseconds"), 0);
