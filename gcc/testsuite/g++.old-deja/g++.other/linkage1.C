// { dg-do assemble  }
typedef struct {
  int i;
} *p;

void f (p) { }			// { dg-error "uses anonymous type" }
p q;				// { dg-error "uses anonymous type" } 

int main()
{
  extern p j;			// { dg-error "uses anonymous type" }
  struct A { int j; };
  extern A a;			// { dg-error "uses local type" }
  extern void f (A);		// { dg-error "uses local type" }
}
