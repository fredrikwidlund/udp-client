#ifndef OUTPUT_H_INCLUDED
#define OUTPUT_H_INCLUDED

typedef struct output output;
struct output
{
  int x;
};

int  output_construct(output *, json_t *);
void output_destruct(output *);

#endif /* OUTPUT_H_INCLUDED */
