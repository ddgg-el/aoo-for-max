|Pd|Max|
|--|---|
|[t_signal](#t_signal)|[t_signal](#t_signal)|
|[t_pd](#t_pd)||
|[t_symbol](#t_symbol)||
|[t_atom](#t_atom)||
|SETSYMBOL|atom_setsym|
|SETFLOAT|atom_setfloat|
|A_SYMBOL|A_SYM|
|t_pd|t_class|
|pd_new|object_alloc|
|t_method|method|
|w_symbol|w_sym|
|pd_error|object_error|
|clock_getlogicaltime|gettime|

**PD structures all defined in m_pd.h**

### t_signal
#### Pd
``` c++
typedef struct _signal
{
    int s_n;            /* number of points in the array */
    t_sample *s_vec;    /* the array */
    t_float s_sr;         /* sample rate */
    int s_refcount;     /* number of times used */
    int s_isborrowed;   /* whether we're going to borrow our array */
    struct _signal *s_borrowedfrom;     /* signal to borrow it from */
    struct _signal *s_nextfree;         /* next in freelist */
    struct _signal *s_nextused;         /* next in used list */
    int s_vecsize;      /* allocated size of array in points */
} t_signal;
```
#### Max
```c++
// z_dsp.h
typedef struct _signal
{
    long s_n;						///< The vector size of the signal.
    t_sample *s_vec;				///< A buffer holding the vector of audio samples.
    float s_sr;						///< The sample rate of the signal.
    struct _signal *s_next;
    struct _signal *s_nextused;
    short s_refcount;
    short s_size;					// element size (* s_n == memory)
    char *s_ptr;					// what to free
} t_signal;
```
### t_pd
#### Pd
```c++
typedef t_class *t_pd;      /* pure datum: nothing but a class pointer */
```
#### Max
```c++
//ext_mess.h
typedef struct maxclass
{
	struct symbol *c_sym;			///< symbol giving name of class
	struct object **c_freelist;		// linked list of free ones
	method c_freefun;				// function to call when freeing
	t_getbytes_size c_size;			// size of an instance
	char c_tiny;					// flag indicating tiny header
	char c_noinlet;					// flag indicating no first inlet for patcher
	struct symbol *c_filename;		///< name of file associated with this class
	t_messlist *c_newmess;			// constructor method/type information
	method c_menufun;				// function to call when creating from object pallette (default constructor)
	long c_flags;					// class flags used to determine context in which class may be used
	long c_messcount;				// size of messlist array
	t_messlist *c_messlist;			// messlist arrray
	void *c_attributes;				// t_hashtab object
	void *c_extra;					// t_hashtab object
	long c_obexoffset;				// if non zero, object struct contains obex pointer at specified byte offset
	void *c_methods;				// methods t_hashtab object
	method c_attr_get;				// if not set, NULL, if not present CLASS_NO_METHOD
	method c_attr_gettarget;		// if not set, NULL, if not present CLASS_NO_METHOD
	method c_attr_getnames;			// if not set, NULL, if not present CLASS_NO_METHOD
	struct maxclass *c_superclass;	// a superclass point if this is a derived class
} t_class;
```

#### t_symbol
```c++
typedef struct _symbol
{
    const char *s_name;
    struct _class **s_thing;
    struct _symbol *s_next;
} t_symbol;
```

#### t_atom
```c++
typedef struct _atom
{
    t_atomtype a_type;
    union word a_w;
} t_atom;
```