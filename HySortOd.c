#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

// Global variable
int DIM;

// compile
// gcc HySortOd.c -lm -o HySortOd

// run example with 10 lines 3 dimensions 4 parititions per dim(Bin) 100(MINSPLIT) No normalization(0) dataset_fixed(dataset)
// ./HySortOd 10 3 4 100 0 dataset_fixed.txt

// Custom structures
struct hypercube
{
    int *dimensions;
};

struct Hypercube
{
    struct hypercube *grid;
    int count;
    int *instancesCount;
};

struct Node
{
    int coordinate;
    int startIndex;
    int endIndex;
};

struct treeNode
{
    struct Node curNode;
    struct treeNode *nextLevel;
    struct treeNode *nextNode;
};

// function prototypes
int importDataset(char *fname, int N, double **dataset, int DIM);
void normalizeDataset(double **dataset, int N, int DIM);
int isPresent(struct Hypercube *hypercube, struct hypercube *curHypercube, int DIM);
void appendHypercube(struct Hypercube **hypercube, struct hypercube *curHypercube, int DIM);
void createHypercube(struct Hypercube **hypercube, double **dataset, int N, int DIM, int BIN);
int compfn(const void *a, const void *b);
void sort(struct Hypercube **hypercube);
void appendNode(struct treeNode *root, int begin, int end, int coordinate);
void buildTree(struct Hypercube *hypercube, struct treeNode *root, int DIM, int curDim, int MINSPLIT);
int neighborhood_density(struct Hypercube *hypercube, struct treeNode *root, int hypercubeIndex, int curDim);
void outlierScore(struct Hypercube *hypercube, struct treeNode *root, int *neighborDensity, int *maxDensity, int N, int DIM);
void printOutlierScore(int *density, int maxDensity, int count);

int main(int argc, char **argv)
{
    // Process command-line arguments
    int N;
    int BIN;
    int MINSPLIT;
    int NORMALIZE;
    char inputFname[500];

    if (argc != 7)
    {
        fprintf(stderr, "Please provide the following on the command line: N (number of lines in the file), dimensionality (number of coordinates per point/feature vector), BIN (Bin parameter), Min Split(Threshold), Normalize (0 or 1)dataset filename. Your input: %s\n", argv[0]);
        return 0;
    }

    sscanf(argv[1], "%d", &N);
    sscanf(argv[2], "%d", &DIM);
    sscanf(argv[3], "%d", &BIN);
    sscanf(argv[4], "%d", &MINSPLIT);
    sscanf(argv[5], "%d", &NORMALIZE);
    strcpy(inputFname, argv[6]);

    // pointer to entire dataset
    double **dataset;

    if (N < 1 || DIM < 1 || BIN < 1 || NORMALIZE > 1 || NORMALIZE < 0)
    {
        printf("\nOne of the following are invalid: N, DIM, BIN , NORMALIZE\n");
        return 0;
    }
    else
    {

        printf("\nNumber of lines (N): %d, Dimensionality: %d, BIN Size: %d, MinSplit: %d,Normalize: %d, Filename: %s\n", N, DIM, BIN, MINSPLIT, NORMALIZE, inputFname);

        // allocate memory for dataset
        dataset = (double **)malloc(sizeof(double *) * N);
        for (int i = 0; i < N; i++)
        {
            dataset[i] = (double *)malloc(sizeof(double) * DIM);
        }

        int ret = importDataset(inputFname, N, dataset, DIM);

        if (ret == 1)
        {
            return 0;
        }
    }

    // Normalize dataset only if required
    if (NORMALIZE == 1)
    {
        normalizeDataset(dataset, N, DIM);
    }

    struct Hypercube *Hypercube = NULL;
    createHypercube(&Hypercube, dataset, N, DIM, BIN);
    sort(&Hypercube);

    struct treeNode *root = malloc(sizeof(struct treeNode));
    root->curNode.coordinate = -1;
    root->curNode.startIndex = 0;
    root->curNode.endIndex = (Hypercube->count) - 1;
    root->nextLevel = NULL;
    root->nextNode = NULL;

    buildTree(Hypercube, root, DIM, 0, MINSPLIT);
    int *density = malloc(sizeof(int) * Hypercube->count);
    int maxDensity = -1;
    outlierScore(Hypercube, root->nextLevel, density, &maxDensity, N, DIM);
    printOutlierScore(density, maxDensity, Hypercube->count);

    // free dataset
    for (int i = 0; i < N; i++)
    {
        free(dataset[i]);
    }

    free(dataset);
    return 0;
}

int importDataset(char *fname, int N, double **dataset, int DIM)
{

    FILE *fp = fopen(fname, "r");

    if (!fp)
    {
        printf("Unable to open file\n");
        return (1);
    }

    char buf[4096];
    int rowCnt = 0;
    int colCnt = 0;
    while (fgets(buf, 4096, fp) && rowCnt < N)
    {
        colCnt = 0;

        char *field = strtok(buf, ",");
        double tmp;
        sscanf(field, "%lf", &tmp);
        dataset[rowCnt][colCnt] = tmp;

        while (field && (colCnt < DIM - 1))
        {
            colCnt++;
            field = strtok(NULL, ",");

            if (field != NULL)
            {
                double tmp;
                sscanf(field, "%lf", &tmp);
                dataset[rowCnt][colCnt] = tmp;
            }
        }
        rowCnt++;
    }

    fclose(fp);
    return 0;
}

// Function to normalize dataset 
void normalizeDataset(double **dataset, int N, int DIM)
{
    double minValue;
    double maxValue;

    for (int i = 0; i < DIM; i++)
    {
        minValue = dataset[0][i];
        maxValue = dataset[0][i];

        for (int j = 1; j < N; j++)
        {
            if (dataset[j][i] > maxValue)
            {
                maxValue = dataset[j][i];
            }

            if (dataset[j][i] < minValue)
            {
                minValue = dataset[j][i];
            }
        }

        for (int j = 0; j < N; j++)
        {
            dataset[j][i] = (dataset[j][i] - minValue) / (maxValue - minValue);
        }
    }
}

// Function to check if the hypercube is already present
int isPresent(struct Hypercube *hypercube, struct hypercube *curHypercube, int DIM)
{
    int found = -1;

    if (hypercube == NULL)
    {
        return found;
    }
    for (int i = 0; i < hypercube->count; i++)
    {
        found = i;
        for (int j = 0; j < DIM; j++)
        {
            if (hypercube->grid[i].dimensions[j] != curHypercube->dimensions[j])
            {
                found = -1;
                break;
            }
        }
        if (found != -1)
        {
            return found;
        }
    }
    return found;
}

// Function to append new hypercube into array of hypercubes(H)
void appendHypercube(struct Hypercube **hypercube, struct hypercube *curHypercube, int DIM)
{
    if (*hypercube == NULL)
    {
        (*hypercube) = malloc(sizeof(struct Hypercube));
        (*hypercube)->count = 0;
        (*hypercube)->grid = NULL;
        (*hypercube)->instancesCount = NULL;
    }

    (*hypercube)->grid = realloc((*hypercube)->grid, sizeof(struct hypercube) * ((*hypercube)->count + 1));
    (*hypercube)->instancesCount = realloc((*hypercube)->instancesCount, sizeof(int) * ((*hypercube)->count + 1));
    (*hypercube)->grid[(*hypercube)->count] = *curHypercube;
    (*hypercube)->instancesCount[(*hypercube)->count] = 1;
    (*hypercube)->count++;

    return;
}

// Function to create array of hypercubes from dataset
void createHypercube(struct Hypercube **hypercube, double **dataset, int N, int DIM, int BIN)
{

    for (int i = 0; i < N; i++)
    {
        struct hypercube *curHypercube = malloc(sizeof(struct hypercube));
        curHypercube->dimensions = malloc(sizeof(int) * DIM);

        for (int j = 0; j < DIM; j++)
        {
            curHypercube->dimensions[j] = (int)floor(dataset[i][j] * BIN);
        }

        int index = isPresent(*hypercube, curHypercube, DIM);

        if (index == -1)
        {
            appendHypercube(hypercube, curHypercube, DIM);
        }
        else
        {
            (*hypercube)->instancesCount[index]++;
        }
    }
    return;
}

// Comparision function for sorting
int compfn(const void *a, const void *b)
{
    for (int i = 0; i < DIM; i++)
    {
        int dimA = (*(struct hypercube *)a).dimensions[i];
        int dimB = (*(struct hypercube *)b).dimensions[i];
        if (dimA != dimB)
        {
            return (dimA - dimB);
        }
    }
    return 0;
}

// Sort the array of hypercubes using default function
void sort(struct Hypercube **hypercube)
{
    qsort((*hypercube)->grid, (*hypercube)->count, sizeof(struct hypercube), compfn);
    return;
}

// Function to append nodes into tree
void appendNode(struct treeNode *root, int begin, int end, int coordinate)
{
    struct treeNode *myNode = malloc(sizeof(struct treeNode));
    myNode->curNode.coordinate = coordinate;
    myNode->curNode.startIndex = begin;
    myNode->curNode.endIndex = end;
    myNode->nextLevel = NULL;
    myNode->nextNode = NULL;

    if (root->nextLevel == NULL)
    {
        root->nextLevel = myNode;
    }
    else
    {
        struct treeNode *temp = root->nextLevel;
        while (temp->nextNode != NULL)
        {
            temp = temp->nextNode;
        }

        temp->nextNode = myNode;
    }
    return;
}

// Function to build tree
void buildTree(struct Hypercube *hypercube, struct treeNode *root, int DIM, int curDim, int MINSPLIT)
{
    if (root == NULL || curDim >= DIM)
    {
        return;
    }

    struct treeNode *temp = root;

    while (temp != NULL)
    {
        int begin = temp->curNode.startIndex;
        int end = temp->curNode.endIndex;
        int curValue = hypercube->grid[begin].dimensions[curDim];

        for (int i = begin; i <= end; i++)
        {
            if (hypercube->grid[i].dimensions[curDim] > curValue)
            {
                if ((i - begin) > MINSPLIT)
                {
                    appendNode(temp, begin, i - 1, curValue);
                }
                begin = i;
                curValue = hypercube->grid[i].dimensions[curDim];
            }

            if (i == end && (i - begin + 1) > MINSPLIT)
            {
                appendNode(temp, begin, i, curValue);
            }
        }

        temp = temp->nextNode;
    }
    buildTree(hypercube, root->nextLevel, DIM, curDim + 1, MINSPLIT);
    return;
}

// Function to calculate neighborhood density for a given hypercube
int neighborhood_density(struct Hypercube *hypercube, struct treeNode *root, int hypercubeIndex, int curDim)
{
    int curDensity = 0;

    if (root->nextLevel == NULL)
    {
        while (root != NULL)
        {
            for (int i = root->curNode.startIndex; i <= root->curNode.endIndex; i++)
            {
                int curDiff = abs(hypercube->grid[hypercubeIndex].dimensions[curDim] - hypercube->grid[i].dimensions[curDim]);
                if (curDiff <= 1)
                {
                    curDensity += hypercube->instancesCount[i];
                }
            }
            root = root->nextNode;
        }
    }
    else
    {
        while (root != NULL)
        {
            int curDiff = abs(root->curNode.coordinate - hypercube->grid[hypercubeIndex].dimensions[curDim]);
            if (curDiff <= 1)
            {
                curDensity += neighborhood_density(hypercube, root->nextLevel, hypercubeIndex, curDim + 1);
            }
            root = root->nextNode;
        }
    }
    return curDensity;
}

// Function to find max density and calculate neighborhood density of each hypercube
void outlierScore(struct Hypercube *hypercube, struct treeNode *root, int *neighborDensity, int *maxDensity, int N, int DIM)
{
    for (int i = 0; i < hypercube->count; i++)
    {
        neighborDensity[i] = neighborhood_density(hypercube, root, i, 0);
        if (neighborDensity[i] > *maxDensity)
        {
            *maxDensity = neighborDensity[i];
        }
    }
    return;
}

// Function to print the outlier score of all hypercubes
// All instances in hypercubes have same outlier score
void printOutlierScore(int *density, int maxDensity, int count)
{
    for (int i = 0; i < count; i++)
    {
        printf("Hypercube index:%d Outlier score: %lf\n", i, (float)(maxDensity - density[i]) / (float)maxDensity);
    }

    return;
}