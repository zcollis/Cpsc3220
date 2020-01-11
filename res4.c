// CPSC 3220 Fall 2017 example resource allocation code
//
// version 2 with:
//   - ability to request/release up to four resources at once
//   - average elapsed time is calculated to worker threads
//
// compile with "gcc -Wall res.c -pthread" if source file is named res.c
// run with "./a.out" or "valgrind --tool=helgrind ./a.out"


#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>


#define THREADS 12
#define RESOURCES 3


// array of elapsed times, one per worker thread

double elapsed[THREADS];


// resource id vector to hold up to four resources; first entry is
//     count of resources and remaining entries are resource ids

typedef int resource_id_vector[5];


// resource type object

typedef struct resource_type_tag{
    int type;                  // field to distinguish resource type
    int total_count;           // total number of resources of this type
    int available_count;       // number of currently available resources
    char *status;              // pointer to status vector on heap; encoding:
                               //     0 = available, 1 = in use
    char *owner;               // pointer to owner vector on heap; encoding:
                               //     0 to 127 are valid owner ids,
                               //     -1 is invalid owner id
    int signature;             // signature for run-time type checking; put
                               //     after a vector to catch some overflows

    // mutual exclusion lock for synchronization
    pthread_mutex_t lock;
    // condition variable for synchronization
    pthread_cond_t cond;

    // methods other than init (constructor) and reclaim (destructor)
    void (*print)( struct resource_type_tag *self );
    int (*allocate)( struct resource_type_tag *self, int tid,
        resource_id_vector *rv );
    void (*release)( struct resource_type_tag *self, int tid,
        resource_id_vector *rv );
} resource_t;


void resource_error( int code ){
    switch( code ){
        case 0:  printf( "**** malloc error for resource object\n" );
                 break;
        case 1:  printf( "**** malloc error for status vector\n" );
                 break;
        case 2:  printf( "**** malloc error for owner vector\n" );
                 break;
        case 3:  printf( "**** could not initialize lock for resource\n" );
                 break;
        case 4:  printf( "**** could not initialize condition for resource\n" );
                 break;
        case 5:  printf( "**** reclaim argument is not a valid resource\n" );
                 break;
        case 6:  printf( "**** print argument is not a valid resource\n" );
                 break;
        case 7:  printf( "**** allocate signature check failed\n" );
                 break;
	case 8:  printf( "**** search for resource failed\n" );
		 break;
	case 9:  printf( "**** release signature check failed\n" );
	         break;
	case 10: printf( "**** release rid bounds check failed\n" );
		 break;
	case 11: printf( "**** release ownership check failed\n" );
		 break;
        default: printf( "**** unknown error code\n" );
    }
    exit( 0 );
}


int resource_check( resource_t *r ){
    if( r->signature == 0x1E5041CE ) return 0;
    else return 1;
}


void resource_reclaim( resource_t *r ){
    if( resource_check( r ) ) resource_error( 5 );
    // call to pthread_cond_destroy()
    pthread_cond_destroy(&(r->cond));
    // call to pthread_mutex_destroy()
    pthread_mutex_destroy(&(r->lock));

    free( r->owner );
    free( r->status );
    free( r );
}


void resource_print( struct resource_type_tag *self ){
    int i;

    // enter critical section
    pthread_mutex_lock(&(self->lock));

    if( resource_check( self ) )          // signature check
        resource_error( 6 );

    printf( "-- resource table for type %d --\n", self->type );
    for(i = 0; i < self->total_count; i++){
        printf(" resource #%d: %d,%d\n", i, self->status[i], self->owner[i] );
    }
    printf("-------------------------------\n");

    // exit critical section
    pthread_mutex_unlock(&(self->lock));

}


int resource_allocate( struct resource_type_tag *self, int tid,
    resource_id_vector *rv ){
    int i; //rid

    // enter critical section
    pthread_mutex_lock(&(self->lock));        // lock mutex object

    if( resource_check( self ) )              // signature check
        resource_error( 7 );

    if( (*rv)[0] > self->total_count ){       // limit check request
        // exit ctirictal section
        pthread_mutex_unlock(&(self->lock));          //  unlock mutex lock

        return 1;
    }

    // fill in
    //
    // access resource request count as (*rv)[0]
    // access first resource id as (*rv)[1]
    // access second request count as (*rv)[2]
    // etc.
    // printf("Allocating tid %d...\n", tid);
    // printf("%d\n",(*rv)[0]);
    // printf("Available count: %d\n", self -> available_count);
    int count = 0; //how many fulfilled requests

      while (self->available_count < (*rv)[0])
      {
        pthread_cond_wait(&(self->cond), &(self->lock));
      }
      for(i = 0; i <= RESOURCES; i++)
      {
        if(count < (*rv)[0] && self->status[i] == 0)
        {
          self -> status[i] = 1;
          self -> owner[i] = tid;
          self -> available_count--;
          count++;
          (*rv)[count] = i;
        }
      }

    //printf("Allocated tid %d...\n", tid);

    // assertion before allocating: self->available_count >= (*rv)[0]

    // exit critical section
    pthread_mutex_unlock(&(self->lock));          //  unlock mutex lock
    //printf("This runs\n");


    return 0;
}


void resource_release( struct resource_type_tag *self, int tid,
    resource_id_vector *rv ){
    int i; //rid;
    // enter critical section
    pthread_mutex_lock(&(self->lock));        // lock mutex object

    // fill in
    //
    // access resource request count as (*rv)[0]
    // access first resource id as (*rv)[1]
    // access second request count as (*rv)[2]
    // etc.
    printf("Releasing %d resources from %d\n", (*rv)[0], tid);
    for (i = 1; i <= (*rv)[0]; i++)
    {
      printf("Releasing resource %d\n",(*rv)[i]);
      if(self -> owner[(*rv)[i]] == tid)
      {
        self -> owner[(*rv)[i]] = -1;
        self -> status[(*rv)[i]] = 0;
        self -> available_count++;
      }
      pthread_cond_broadcast(&(self->cond));
    }

    //printf("Available count after release: %d\n", self -> available_count);

    // exit critical section
    pthread_mutex_unlock(&(self->lock));          //  unlock mutex lock
    //printf("This runs too\n");
}


resource_t * resource_init( int type, int total ){
    resource_t *r;
    int i, rc;

    r = malloc( sizeof( resource_t ) );   // obtain memory for struct
    if( r == NULL ) resource_error( 0 );

    r->type = type;                       // set data fields
    r->total_count = total;
    r->available_count = total;

    r->status = malloc( total + 1 );      // obtain memory for vector
    if( r->status == NULL )               //     extra entry allows
        resource_error( 1 );              //     faster searching
    for( i = 0; i <= total; i++ )         // 0 = available, 1 = in use
        r->status[i] = 0;

    r->owner = malloc( total );           // obtain memory for vector
    if( r->owner == NULL )
        resource_error( 2 );
    for( i = 0; i < total; i++ )          // -1 = invalid owner id
        r->owner[i] = -1;

    r->signature = 0x1E5041CE;

    // call to pthread_mutex_init() with rc as return code
    rc = pthread_mutex_init(&(r->lock), NULL);
    if( rc != 0 ) resource_error( 3 );

    // call to pthread_cond_init() with rc as return code
    rc = pthread_cond_init(&(r->cond), NULL);
    if( rc != 0 ) resource_error( 4 );

    r->print = &resource_print;           // set method pointers
    r->allocate = &resource_allocate;
    r->release = &resource_release;

    return r;
}


// argument vector for threads

typedef struct{
    resource_t *rp;
    int id;
} argvec_t;


// worker thread skeleton

void *worker( void *ap ){
    resource_t *resource = ((argvec_t *)ap)->rp;
    int thread_id = ((argvec_t *)ap)->id;
    resource_id_vector rv;
    int rc;
    struct timespec start, finish;

    clock_gettime( CLOCK_MONOTONIC, &start );

    rv[0] = 1 + (thread_id & 3);

    rc = (resource->allocate)( resource, thread_id, &rv );
    if( rc != 0 ) return NULL;

    switch( rv[0] ){
        case 1:
            printf( "thread #%d uses resource #%d\n", thread_id, rv[1] );
            break;
        case 2:
            printf( "thread #%d uses resources #%d and #%d\n",
                thread_id, rv[1], rv[2] );
            break;
        case 3:
            printf( "thread #%d uses resources #%d, #%d, and #%d\n",
                thread_id, rv[1], rv[2], rv[3] );
            break;
        case 4:
            printf( "thread #%d uses resources #%d, #%d, #%d, and #%d\n",
                thread_id, rv[1], rv[2], rv[3], rv[4] );
    }
    if( thread_id == 0 ) sleep( 10 );
    else sleep( 1 );

    (resource->release)( resource, thread_id, &rv );

    clock_gettime( CLOCK_MONOTONIC, &finish );
    elapsed[thread_id] = finish.tv_sec - start.tv_sec;
    elapsed[thread_id] += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;

    return NULL;
}


// observer thread

void *observer( void *ap ){
    resource_t *resource = ((argvec_t *)ap)->rp;
    int i;

    for( i = 0; i < 2; i++ ){
        sleep( 2 );
        (resource->print)( resource );
    }

    return NULL;
}


// test driver

int main(int argc, char **argv){
    pthread_t threads[ THREADS + 1 ];
    argvec_t args[ THREADS + 1 ];
    resource_t *resource_1;
    double avg_elapsed;
    int i;

    resource_1 = resource_init( 1, RESOURCES );

    (resource_1->print)( resource_1 );

    for( i = 0; i < THREADS; i++ ){
        args[i].rp = resource_1;
        args[i].id = i;
        if( pthread_create( &threads[i], NULL, &worker, (void *)(&args[i]) ) ){
            printf("**** could not create worker thread %d\n", i);
            exit( 0 );
        }
    }

    i = THREADS;
    args[i].rp = resource_1;
    args[i].id = i;
    if( pthread_create( &threads[i], NULL, &observer, (void *)(&args[i]) ) ){
        printf("**** could not create observer thread\n");
        exit( 0 );
    }

    for( i = 0; i < (THREADS+1); i++ ){
        if( pthread_join( threads[i], NULL ) ){
            printf( "**** could not join thread %d\n", i );
            exit( 0 );
        }
    }

    (resource_1->print)( resource_1 );

    resource_reclaim( resource_1 );

    avg_elapsed = 0.0;
    for( i = 0; i < THREADS; i++ ) avg_elapsed += elapsed[i];
    avg_elapsed /= (double) THREADS;
    printf( "average elapsed time for worker threads is %5.1f\n", avg_elapsed );

    return 0;
}
