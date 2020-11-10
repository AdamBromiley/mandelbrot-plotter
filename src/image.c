#include "image.h"

#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libgroot/include/log.h"
#include "percy/include/parser.h"

#include "array.h"
#include "connection_handler.h"
#include "ext_precision.h"
#include "function.h"
#include "parameters.h"
#include "request_handler.h"


#define IMAGE_HEADER_LEN_MAX 128


/* Minimum/maximum memory limit values */
const size_t MEMORY_MIN = 1000;
const size_t MEMORY_MAX = SIZE_MAX;

/* Minimum/maximum allowable thread count */
const unsigned int THREAD_COUNT_MIN = 1;
const unsigned int THREAD_COUNT_MAX = 512;


static void blockToImage(const Block *block);


/* Create image file and write header */
int initialiseImage(PlotCTX *p, const char *filepath)
{
    logMessage(DEBUG, "Opening image file \'%s\'", filepath);

    p->file = fopen(filepath, "wb");

    if (!p->file)
    {
        logMessage(ERROR, "File \'%s\' could not be opened", filepath);
        return 1;
    }

    logMessage(DEBUG, "Image file successfully opened");

    if (p->output == OUTPUT_PNM)
    {
        char header[IMAGE_HEADER_LEN_MAX];

        logMessage(DEBUG, "Writing header to image");

        /* Write PNM file header */
        switch (p->colour.depth)
        {
            case BIT_DEPTH_1:
                /* PBM file */
                snprintf(header, sizeof(header), "P4 %zu %zu ", p->width, p->height);
                break;
            case BIT_DEPTH_8:
                /* PGM file */
                snprintf(header, sizeof(header), "P5 %zu %zu 255 ", p->width, p->height);
                break;
            case BIT_DEPTH_24:
                /* PPM file */
                snprintf(header, sizeof(header), "P6 %zu %zu 255 ", p->width, p->height);
                break;
            default:
                logMessage(ERROR, "Could not determine bit depth");
                return 1;
        }

        fprintf(p->file, "%s", header);

        logMessage(DEBUG, "Header \'%s\' successfully wrote to image", header);
    }

    return 0;
}


/* Initialise plot array, run function, then write to file */
int imageOutput(PlotCTX *p, size_t mem, unsigned int threadCount)
{
    /* Pointer to fractal generation function */
    void * (*genFractal)(void *);

    /* Array blocks */
    ArrayCTX *array;
    Block *block;

    /* Processing threads */
    Thread *threads;
    Thread *thread;

    switch (precision)
    {
        case STD_PRECISION:
            genFractal = generateFractal;
            break;
        case EXT_PRECISION:
            genFractal = generateFractalExt;
            break;
        
        #ifdef MP_PREC
        case MUL_PRECISION:
            genFractal = generateFractalMP;
            break;
        #endif
        
        default:
            return 1;
    }

    /* Create array metadata struct */
    array = createArrayCTX(p);

    if (!array)
        return 1;

    /* Allocate memory to the array in manageable blocks */
    block = mallocArray(array, mem);

    if (!block)
    {
        freeArrayCTX(array);
        return 1;
    }

    /* 
     * Create a list of processing threads.
     * The most optimised solution is one thread per processing core.
     */
    threads = createThreads(block, threadCount);

    if (!threads)
    {
        freeArrayCTX(array);
        freeBlock(block);
        return 1;
    }

    /*
     * Because image dimensions can lead to billions of pixels, the plot array
     * may not be able to be stored in one whole memory chunk. Therefore, as per
     * the preceding functions, a block size is determined. A block is a section
     * of N rows of the image array that allow threads will perform on at once.
     * Once all threads have finished, the block gets written to the image file
     * and the cycle continues. The array may not divide evenly into blocks, so
     * the reminader rows are calculated prior and stored in the block context
     * structure
     */
    for (block->id = 0; block->id <= block->ctx->count; ++(block->id))
    {
        if (block->id == block->ctx->count)
        {
            /* If there are no remainder rows */
            if (!(block->ctx->remainder))
                break;

            /* If there's remaining rows */
            block->rows = block->ctx->remainder;
        }
        else
        {
            block->rows = block->ctx->rows;
        }

        logMessage(INFO, "Working on block %u (%zu rows)", block->id, block->rows);

        /* Create threads to significantly decrease execution time */
        for (unsigned int i = 0; i < threads->ctx->count; ++i)
        {
            thread = &(threads[i]);
            logMessage(INFO, "Spawning thread %u", thread->tid);
    
            if (pthread_create(&(thread->pid), NULL, genFractal, thread))
            {
                logMessage(ERROR, "Thread could not be created");
                freeArrayCTX(array);
                freeBlock(block);
                freeThreads(threads);
                return 1;
            }
        }

        logMessage(INFO, "All threads successfully created");
        
        /* Wait for threads to exit */
        for (unsigned int i = 0; i < threads->ctx->count; ++i)
        {
            thread = &(threads[i]);

            if (pthread_join(thread->pid, NULL))
            {
                logMessage(ERROR, "Thread %u could not be harvested", thread->tid);
                freeArrayCTX(array);
                freeBlock(block);
                freeThreads(threads);
                return 1;
            }
                
            logMessage(INFO, "Thread %u joined", thread->tid);
        }

        logMessage(INFO, "All threads successfully destroyed");

        blockToImage(block);
    }

    logMessage(DEBUG, "Freeing memory");

    freeArrayCTX(array);
    freeBlock(block);
    freeThreads(threads);

    return 0;
}


/* Initialise plot array, run function, then write to file */
int imageOutputMaster(PlotCTX *p, LANCTX *lan, size_t mem)
{
    /* Array blocks */
    ArrayCTX *array;
    Block *block;

    /* Create array metadata struct */
    array = createArrayCTX(p);

    if (!array)
        return 1;

    /* Allocate memory to the array in manageable blocks */
    block = mallocArray(array, mem);

    if (!block)
    {
        freeArrayCTX(array);
        return 1;
    }

    /*
     * Because image dimensions can lead to billions of pixels, the plot array
     * may not be able to be stored in one whole memory chunk. Therefore, as per
     * the preceding functions, a block size is determined. A block is a section
     * of N rows of the image array that allow threads will perform on at once.
     * Once all threads have finished, the block gets written to the image file
     * and the cycle continues. The array may not divide evenly into blocks, so
     * the reminader rows are calculated prior and stored in the block context
     * structure
     */
    for (block->id = 0; block->id <= block->ctx->count; ++(block->id))
    {
        if (block->id == block->ctx->count)
        {
            /* If there are no remainder rows */
            if (!(block->ctx->remainder))
                break;

            /* If there's remaining rows */
            block->rows = block->ctx->remainder;
        }
        else
        {
            block->rows = block->ctx->rows;
        }

        logMessage(INFO, "Working on block %u (%zu rows)", block->id, block->rows);

        if (listener(lan->slaves, lan->n, block))
        {
            freeArrayCTX(array);
            freeBlock(block);
            return 1;
        }

        blockToImage(block);
    }

    logMessage(DEBUG, "Freeing memory");

    freeArrayCTX(array);
    freeBlock(block);

    return 0;
}


/* Initialise plot array, run function, then write to file */
int imageRowOutput(PlotCTX *p, LANCTX *lan, unsigned int threadCount)
{
    /* Pointer to fractal row generation function */
    void * (*genFractalRow)(void *);

    /* Array blocks */
    ArrayCTX *array;

    RowCTX row;

    /* Processing threads */
    SlaveThread *threads;
    SlaveThread *thread;

    size_t rowSize;

    char *writeBuffer;

    switch (precision)
    {
        case STD_PRECISION:
            genFractalRow = generateFractalRow;
            break;
        case EXT_PRECISION:
            genFractalRow = generateFractalRow;
            break;
        
        #ifdef MP_PREC
        case MUL_PRECISION:
            genFractalRow = generateFractalRow;
            break;
        #endif
        
        default:
            return 1;
    }

    /* Create array metadata struct */
    array = createArrayCTX(p);

    if (!array)
        return 1;

    rowSize = (p->colour.depth == BIT_DEPTH_ASCII)
              ? p->width * sizeof(char)
              : p->width * p->colour.depth / 8;

    array->array = malloc(rowSize);

    if (!array->array)
    {
        freeArrayCTX(array);
        return 1;
    }

    row.array = array;

    /* 
     * Create a list of processing threads.
     * The most optimised solution is one thread per processing core.
     */
    threads = createSlaveThreads(&row, threadCount);

    if (!threads)
    {
        freeArrayCTX(array);
        return 1;
    }

    /* For row array plus row number at beginning */
    writeBuffer = malloc(rowSize + 6);

    if (!writeBuffer)
    {
        freeArrayCTX(array);
        freeSlaveThreads(threads);
        return 1;
    }

    /*
     * Because image dimensions can lead to billions of pixels, the plot array
     * may not be able to be stored in one whole memory chunk. Therefore, as per
     * the preceding functions, a block size is determined. A block is a section
     * of N rows of the image array that allow threads will perform on at once.
     * Once all threads have finished, the block gets written to the image file
     * and the cycle continues. The array may not divide evenly into blocks, so
     * the reminader rows are calculated prior and stored in the block context
     * structure
     */
    

    while (1)
    {
        char readBuffer[10];
        char *endptr;

        ssize_t ret;
        
        ret = writeSocket("", lan->s, 1);

        if (ret == 0)
        {
            break;
        }
        else if (ret < 0 || (size_t) ret != 1)
        {
            logMessage(ERROR, "Could not write to socket connection");
            free(writeBuffer);
            freeArrayCTX(array);
            freeSlaveThreads(threads);
            return 1;
        }

        ret = readSocket(readBuffer, lan->s, sizeof(readBuffer));

        if (ret == 0)
        {
            break;
        }
        else if (ret < 0)
        {
            logMessage(ERROR, "Error reading from socket connection");
            free(writeBuffer);
            freeArrayCTX(array);
            freeSlaveThreads(threads);
            return 1;
        }

        stringToUIntMax(&row.row, readBuffer, 0, p->height, &endptr, BASE_DEC);
        memset(readBuffer, '\0', sizeof(readBuffer));

        logMessage(INFO, "Working on row %zu", row.row);

        /* Create threads to significantly decrease execution time */
        for (unsigned int i = 0; i < threads->ctx->count; ++i)
        {
            thread = &(threads[i]);
            logMessage(INFO, "Spawning thread %u", thread->tid);
    
            if (pthread_create(&(thread->pid), NULL, genFractalRow, thread))
            {
                logMessage(ERROR, "Thread could not be created");
                free(writeBuffer);
                freeArrayCTX(array);
                freeSlaveThreads(threads);
                return 1;
            }
        }

        logMessage(INFO, "All threads successfully created");
        
        /* Wait for threads to exit */
        for (unsigned int i = 0; i < threads->ctx->count; ++i)
        {
            thread = &(threads[i]);

            if (pthread_join(thread->pid, NULL))
            {
                logMessage(ERROR, "Thread %u could not be harvested", thread->tid);
                free(writeBuffer);
                freeArrayCTX(array);
                freeSlaveThreads(threads);
                return 1;
            }
                
            logMessage(INFO, "Thread %u joined", thread->tid);
        }

        logMessage(INFO, "All threads successfully destroyed");

        sprintf(writeBuffer, "%zu", row.row);
        memcpy(writeBuffer + 6, array->array, rowSize);

        ret = writeSocket(writeBuffer, lan->s, rowSize + 6);
        memset(writeBuffer, '\0', rowSize + 6);

        if (ret == 0)
        {
            break;
        }
        else if (ret < 0 || (size_t) ret != rowSize + 6)
        {
            logMessage(ERROR, "Could not write to socket connection");
            free(writeBuffer);
            freeArrayCTX(array);
            freeSlaveThreads(threads);
            return 1;
        }

        ret = readSocket(readBuffer, lan->s, sizeof(readBuffer));
        memset(readBuffer, '\0', sizeof(readBuffer));

        if (ret == 0)
        {
            break;
        }
        else if (ret < 0)
        {
            logMessage(ERROR, "Error reading from socket connection");
            free(writeBuffer);
            freeArrayCTX(array);
            freeSlaveThreads(threads);
            return 1;
        }
    }

    logMessage(DEBUG, "Freeing memory");

    free(writeBuffer);
    freeArrayCTX(array);
    freeSlaveThreads(threads);

    return 0;
}


/* Close image file */
int closeImage(PlotCTX *p)
{
    logMessage(DEBUG, "Closing image file");

    if (fclose(p->file))
    {
        logMessage(WARNING, "Image file could not be closed");
        p->file = NULL;
        return 1;
    }

    p->file = NULL;

    logMessage(DEBUG, "Image file closed");

    return 0;
}


/* Write block to image file */
static void blockToImage(const Block *block)
{
    PlotCTX *p = block->ctx->array->params;

    void *array = block->ctx->array->array;

    size_t arrayLength = block->rows * p->width;
    double pixelSize = (p->colour.depth == BIT_DEPTH_ASCII)
                       ? sizeof(char)
                       : p->colour.depth / 8.0;
    size_t arraySize = arrayLength * pixelSize;

    FILE *image = p->file;

    logMessage(INFO, "Writing %zu pixels (%zu bytes; pixel size = %d bits) to image file",
                     arrayLength, arraySize, p->colour.depth);

    if (p->colour.depth != BIT_DEPTH_ASCII)
    {
        fwrite(array, sizeof(char), arraySize, image);
    }
    else
    {
        for (size_t i = 0; i < block->rows; ++i)
        {
            const char *LINE_TERMINATOR = "\n";

            fwrite(array, sizeof(char), p->width, image);
            fwrite(LINE_TERMINATOR, sizeof(*LINE_TERMINATOR), strlen(LINE_TERMINATOR), image);

            array = (char *) array + p->width;
        }
    }

    logMessage(INFO, "Block successfully wrote to file");

    return;
}