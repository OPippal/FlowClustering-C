#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define IP_MAX_LENGTH 16        
#define INF_DISTANCE 1000000.0  // big number used as "infinity" for distances


typedef struct 
{
    int flow_ID;                        // unique ID of the flow
    char src_IP[IP_MAX_LENGTH];         // source IP address (as string)
    char dst_IP[IP_MAX_LENGTH];         // destination IP address (as string)
    int total_bytes;                    // total number of bytes in the flow
    int flow_duration;                  // duration of the flow
    int packet_count;                   // number of packets in the flow
    double average_interarrival_time;   // average time between packets
    double s;                           // average size of one packet (total_bytes / packet_count)
} Flow;

typedef struct 
{
    unsigned int size;      // how many flow indices are in this cluster
    int* indices;           // dynamic array of indices (indexes into Flow array)
    unsigned int capacity;  // current capacity of the indices array
} Cluster;

/*
 * Function: load_count
 * Reads the number of flows from the file.
 * Expected format: "count=NUMBER"
 * file- opened file pointer
 * returns: the count of flows if ok,
 * 0 if there is an error (invalid format or file is NULL).
 */
int load_count(FILE* file)
{
    int count;

    if (file != NULL)
    {
        int result_count = fscanf(file, "count=%d", &count);

        if (result_count != 1)
        {
            // invalid header format
            fprintf(stderr, "INVALID INPUT FORMAT");
            return 0;
        }

        return count;
    }
    else
    {
        // file pointer is NULL
        fprintf(stderr, "CANT READ FILE");
        return 0;
    }
}

/*
 * Function: load_data
 * Reads all flows from the file into the array.
 * It also computes 's' for each flow.
 * file - opened file pointer
 * flow - array of Flow with size at least 'count'
 * count - number of flows to read
 *
 * returns: 1 if success
 *          0 if there is an error (invalid input format or division by zero).
 */
int load_data(FILE* file, Flow* flow, unsigned int count)
{
    for (unsigned int i = 0; i < count; i++)
    {
        /*
         * %15s is used to avoid buffer overflow.
         * We have IP_MAX_LENGTH = 16, so max 15 chars + '\0'.
         */
        int result_count = fscanf(
            file,
            "%d %15s %15s %d %d %d %lf",
            &flow[i].flow_ID,
            flow[i].src_IP,
            flow[i].dst_IP,
            &flow[i].total_bytes,
            &flow[i].flow_duration,
            &flow[i].packet_count,
            &flow[i].average_interarrival_time
        );

        if (result_count != 7)
        {
            // some value is missing or format is wrong
            fprintf(stderr, "INVALID INPUT FORMAT");
            return 0;
        }

        if (flow[i].packet_count != 0)
        {
            // compute average size of one packet
            flow[i].s = (double)flow[i].total_bytes / flow[i].packet_count;
        }
        else
        {
            // do not divide by zero, packet count must not be zero
            fprintf(stderr, "DIVIDED BY ZERO");
            return 0;
        }
    }

    return 1;
}

/*
 * Function: cluster_init
 * Initializes 'count' clusters
 * Each cluster starts with one flow (cluster of size 1)
 * cluster - array of clusters with size 'count'
 * count - number of flows
 */
int cluster_init(Cluster* cluster, unsigned int count)
{
    for (unsigned int i = 0; i < count; i++)
    {
        cluster[i].size = 1;
        cluster[i].capacity = 1;

        // allocate space for one index
        cluster[i].indices = (int*)malloc(sizeof(int));
        if (cluster[i].indices == NULL)
        {
            for (unsigned int j = 0; j < i; j++)
            {
                free(cluster[j].indices);
            }
        return 0;
        }
        // this cluster contains flow with index i
        cluster[i].indices[0] = i;
    }
    return 1;
}

/*
 * Function: distance_flows
 * Computes distance between two flows using Euklidovska vzdalenost
 * with given weights
 * a, b - flows to compare
 * WB, WT, WD, WS - weights for bytes, time, delay and packet size
 * returns: distance between two flows
 */
double distance_flows(Flow a, Flow b, double WB, double WT, double WD, double WS)
{
    double diff_b = (double)a.total_bytes - (double)b.total_bytes;
    double diff_t = (double)a.flow_duration - (double)b.flow_duration;
    double diff_d = a.average_interarrival_time - b.average_interarrival_time;
    double diff_s = a.s - b.s;

    double result = sqrt(
        WB * diff_b * diff_b +
        WT * diff_t * diff_t +
        WD * diff_d * diff_d +
        WS * diff_s * diff_s
    );

    return result;
}

/*
 * Function: cluster_distance
 * Computes distance between two clusters.
 * It uses "single linkage" method: distance is minimum distance
 * between any pair of flows from cluster A and cluster B.
 *
 * A, B - pointers to two clusters
 * flow - array with all flows
 * WB, WT, WD, WS - weights for distance_flows
 * returns: minimal distance between flows in A and B,
 *          or INF_DISTANCE if one of clusters is empty.
 */
double cluster_distance(Cluster* A, Cluster* B, Flow* flow,
    double WB, double WT, double WD, double WS)
{
    double min_dist = INF_DISTANCE;

    // If one cluster is empty, we return "infinite" distance.
    if (A->size == 0 || B->size == 0)
    {
        return INF_DISTANCE;
    }

    // Go through all flow pairs (i from A, j from B)
    for (unsigned int i = 0; i < A->size; i++)
    {
        for (unsigned int j = 0; j < B->size; j++)
        {
            int idxA = A->indices[i];
            int idxB = B->indices[j];

            double dist = distance_flows(flow[idxA], flow[idxB], WB, WT, WD, WS);

            if (dist < min_dist)
            {
                min_dist = dist;
            }
        }
    }

    return min_dist;    
}

/*
 * Function: merge_clusters
 * Merges cluster B into cluster A.
 * All indices from B are appended to A
 * Cluster B will be emptied and its memory freed.
 * A, B - pointers to two clusters (A will be the result)
 */
int merge_clusters(Cluster* A, Cluster* B)
{
    int new_size = (int)A->size + (int)B->size;

    // If we need more capacity, we realloc.
    if ((unsigned int)new_size > A->capacity)
    {
        int* new_arr = (int*)realloc(A->indices, 2 * (unsigned int)new_size * sizeof(int));
        if (new_arr == NULL)
        {
            return 0;
        }
        
        A->indices = new_arr;
        A->capacity = 2 * (unsigned int)new_size;
    }

    // copy all indices from B to the end of A
    for (unsigned int i = 0; i < B->size; i++)
    {
        A->indices[A->size + i] = B->indices[i];
    }

    // update size of A
    A->size = (unsigned int)new_size;

    // clear cluster B
    B->size = 0;
    B->capacity = 0;
    free(B->indices);
    B->indices = NULL;
    return 1;
}

/*
 * Simple swap for two integers.
 */
void swap(int* xp, int* yp)
{
    int temp = *xp;
    *xp = *yp;
    *yp = temp;
}

/*
 * Function: bubble_sort
 */
void bubble_sort(int* a, int* b, int k)
{
    for (int i = 0; i < k - 1; i++)
    {
        for (int j = i + 1; j < k; j++)
        {
            if (a[i] > a[j])
            {
                swap(&a[i], &a[j]);
                swap(&b[i], &b[j]);
            }
        }
    }
}

/*
 * Function: sort_cluster_indices
 * Sorts indices inside one cluster by flow_ID.
 * c - pointer to cluster
 * flow - array with all flows (for reading flow_ID)
 */
void sort_cluster_indices(Cluster* c, Flow* flow)
{
    for (unsigned int i = 0; i < c->size - 1; i++)
    {
        for (unsigned int j = i + 1; j < c->size; j++)
        {
            int idx_i = c->indices[i];
            int idx_j = c->indices[j];

            // compare flow IDs, not indices
            if (flow[idx_i].flow_ID > flow[idx_j].flow_ID)
            {
                swap(&c->indices[i], &c->indices[j]);
            }
        }   
    }
}

int main(int argc, char* argv[])
{
    double WB = 0.0;
    double WT = 0.0;
    double WD = 0.0;
    double WS = 0.0;
    int N = 0;
    
    // Check valid number of arguments
    if (argc != 2 && argc != 7)
    {
        fprintf(stderr, "INVALID ARGUMENT COUNT");
        return 0;
    }
    else if (argc == 7)
    {
        // Read N and weights from arguments
        N  = (int)strtol(argv[2], NULL, 10);
        WB = strtod(argv[3], NULL);
        WT = strtod(argv[4], NULL);
        WD = strtod(argv[5], NULL);
        WS = strtod(argv[6], NULL);

        if (N <= 0) 
        { 
            fprintf(stderr, "INVALID ARGUMENT COUNT"); 
            return 0; 
        }

        if (WB < 0 || WT < 0 || WD < 0 || WS < 0) 
        {
            fprintf(stderr, "INVALID INPUT FORMAT");
            return 0;
        }
    }

    // Open input file
    FILE* file = fopen(argv[1], "r");
    if (file == NULL)
    {
        fprintf(stderr, "CANT READ FILE");
        return 0;
    }

    // Load number of flows
    int count = load_count(file);
    if (count <= 0)
    {
        fclose(file);
        return 0;
    }

    if (N > count) {
    fprintf(stderr, "INVALID INPUT FORMAT");
    fclose(file);
    return 0;
    }

    // Allocate memory for all flows
    Flow* flow = (Flow*)malloc(count * sizeof(Flow));
    if (flow == NULL)
    {
        fprintf(stderr, "CANT READ FILE");
        fclose(file);
        return 0;
    }

    // Load flow data
    if (!load_data(file, flow, count))
    {
        fprintf(stderr, "LOAD DATA ERROR");
        fclose(file);
        free(flow);
        return 0;
    }
    fclose(file);
    // Allocate memory for clusters
    Cluster* cluster = (Cluster*)malloc(count * sizeof(Cluster));
    if (cluster == NULL)
    {
        fprintf(stderr, "CANT READ FILE");
        free(flow);
        return 0;
    }

    // Initialize each cluster with one flow
    if (!cluster_init(cluster, count))
    {
        fprintf(stderr, "CANT READ FILE");
        free(flow);
        free(cluster);
        return 0;
    }
    int current_clusters = count;

    // If only file is given, each flow is its own cluster (N = count)
    if (argc == 2) 
    {
        N = count;
    }

    /*
     * Clustering loop:
     * We merge clusters until we have 'N' clusters.
     * We always merge the pair with minimal distance.
     */
    while (current_clusters > N && argc == 7)
    {
        double best_dist = INF_DISTANCE;
        int best_i = -1;
        int best_j = -1;

        // Try all pairs of clusters i < j
        for (int i = 0; i < count - 1; i++)
        {
            for (int j = i + 1; j < count; j++)
            {
                // skip empty clusters
                if (cluster[i].size == 0 || cluster[j].size == 0)
                {
                    continue;
                }

                double dist = cluster_distance(&cluster[i], &cluster[j], flow, WB, WT, WD, WS);

                // If we find better distance, remember this pair
                if (dist < best_dist)
                {
                    best_dist = dist;
                    best_i = i;
                    best_j = j;
                }  
            }
        }

        // If we did not find any valid pair, something is wrong
        if (best_i < 0 || best_j < 0)
        {
            fprintf(stderr, "ERROR");

            // free all allocated memory before exit
            free(flow);

            for (int i = 0; i < count; i++)
            {
                if (cluster[i].indices != NULL)
                {
                    free(cluster[i].indices);
                }
            }
            free(cluster);
            return 0;
        }
        // Merge best pair of clusters
        if (!merge_clusters(&cluster[best_i], &cluster[best_j]))
        {
            fprintf(stderr, "CANT READ FILE");

            // free all allocated memory before exit
            free(flow);

            for (int i = 0; i < count; i++)
            {
                if (cluster[i].indices != NULL)
                {
                    free(cluster[i].indices);
                }
            }
            free(cluster);
            return 0;
        }
        current_clusters--;
    }

    /*
     * Now we prepare arrays for printing clusters in correct order
     * order_index[k] - index of cluster in 'cluster' array
     * order_minID[k] - minimal flow_ID inside that cluster
     * We will sort by order_minID, so clusters with smaller min ID are printed first
     */

    int order_index[N];
    int order_minID[N];
    int k = 0; // number of non-empty clusters

    // For each cluster, find minimal flow_ID
    for (int c = 0; c < count; c++)
    {
        if (cluster[c].size == 0)
        {
            continue;
        }

        order_index[k] = c;

        int idx0 = cluster[c].indices[0];
        int min_ID = flow[idx0].flow_ID;

        for (unsigned int i = 0; i < cluster[c].size; i++)
        {
            int idx = cluster[c].indices[i];
            int id = flow[idx].flow_ID;

            if (id < min_ID)
            {
                min_ID = id;
            }
        }

        order_minID[k] = min_ID;
        k += 1;
    }

    // Sort clusters by minimal flow ID
    bubble_sort(order_minID, order_index, k);

    // Print final clusters
    printf("Clusters:\n");

    for (int pos = 0; pos < k; pos++)
    {
        int c_index = order_index[pos];

        // Sort indices inside cluster by flow_ID
        sort_cluster_indices(&cluster[c_index], flow);

        printf("cluster %d: ", pos);

        for (unsigned int i = 0; i < cluster[c_index].size; i++)
        {
            int fi = cluster[c_index].indices[i];
            printf("%d ", flow[fi].flow_ID);
        }
        printf("\n");
    }

    printf("\n");

    // Free all allocated memory
    free(flow);

    for (int i = 0; i < count; i++)
    {
        if (cluster[i].indices != NULL)
        {
            free(cluster[i].indices);
        }
    }

    free(cluster);
    return 0;
}
