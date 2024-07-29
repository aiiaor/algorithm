#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASCII_CHAR 126
#define BLOCK_SIZE 1024
#define SEARCH_TERM 65
#define MAX_STRING 5000


struct char_mapping{
    unsigned int ascii[127];
};

struct index_decode_data{
    int index;
    int start_position;
};

struct decoded_data{
    char *decoded;
    int decoded_size;
    int index;
    int start_position;
};

struct search_result{
    int first;
    int last;
};

void create_occurence_index(FILE *bwtfile, FILE *idxfile, struct char_mapping cm, int num_block);
void write_block_to_index_file(unsigned int occurence[], FILE *idxfile);
int get_next_bracket(FILE *bwtfile, FILE *idxfile, struct char_mapping cm, unsigned int c_table[],int index);
unsigned int get_occurence(FILE *bwtfile,FILE *idxfile,int c, int char_position,struct char_mapping cm);
struct search_result bwtsearch(FILE *bwtfile, FILE *idxfile, unsigned int c_table[], char* search_pattern, struct char_mapping cm);

struct char_mapping init_char_mapping(){
    struct char_mapping cm;
    cm.ascii[9] = 1; //Tab character
    cm.ascii[10] = 2; //New line character
    cm.ascii[13] = 2; //Carriage return character

    for (int i = 0; i < 32; i++){
        if (i == 9 || i == 10 || i == 13){
            continue;
        }
        cm.ascii[i] = 0;
    }

    for(int i = 32; i <= ASCII_CHAR; i++){
        cm.ascii[i] = i-29;
    }
    return cm;
}

//In case no index file, create new index file
void create_occurence_index(FILE *bwtfile, FILE *idxfile, struct char_mapping cm, int num_block){
    int block_offset = 0;
    int c;

    fseek(bwtfile, 0, SEEK_SET);
    unsigned int occurence[ASCII_CHAR+1];
    for(int i = 0; i <= ASCII_CHAR; i++){
        occurence[i] = 0;
    }

    while((c = fgetc(bwtfile)) != EOF){
        occurence[cm.ascii[(int)c]]++;
        block_offset++;
        if(block_offset == BLOCK_SIZE){
            write_block_to_index_file(occurence, idxfile);
            block_offset = 0;
        }
    }

    if (num_block != 0){
        write_block_to_index_file(occurence, idxfile);
    }
}
    
void write_block_to_index_file(unsigned int occurence[], FILE *idxfile){
    size_t size = (ASCII_CHAR + 1) * sizeof(unsigned int);
    fwrite(occurence, size, 1, idxfile);
}

unsigned int get_occurence(FILE *bwtfile,FILE *idxfile,int c, int char_position,struct char_mapping cm){
    int block_offset = char_position/BLOCK_SIZE;
    unsigned int occurence;
    int ascii_value = c;
    
    int char_map_position = cm.ascii[ascii_value];
    if (block_offset != 0){
        fseek(idxfile, (block_offset-1)*(ASCII_CHAR+1)*sizeof(unsigned int) + char_map_position*sizeof(unsigned int), SEEK_SET);
        fread(&occurence, sizeof(unsigned int), 1, idxfile);
    }else{
        occurence = 0;
    }
    
    int buffer_size = char_position-(block_offset*BLOCK_SIZE);
    char buffer[buffer_size];
    fseek(bwtfile, block_offset*BLOCK_SIZE, SEEK_SET);
    fread(buffer, sizeof(char), buffer_size+1, bwtfile);


    for(int i = 0; i <= buffer_size; i++){
        if(buffer[i] == ascii_value){
            occurence++;
        }
    }

    return occurence;
}

void create_c_table(int num_block, FILE *idxfile, FILE *bwtfile, unsigned int c_table[], struct char_mapping cm){
    unsigned int cumulative = 0;
    int index = 0;
    size_t size = (ASCII_CHAR + 1) * sizeof(unsigned int);
    unsigned int buffer[ASCII_CHAR+1];
    unsigned int c;
    
    
    
    if(num_block != 0){
        fseek(idxfile, num_block * size, SEEK_SET);
        fread(buffer, sizeof(unsigned int), ASCII_CHAR+1, idxfile);
        for(int i = 0; i <=ASCII_CHAR; i++){
            c = buffer[i];
            
            c_table[index] = cumulative;
            cumulative += c;
            index++;
        }
    }else{
        //small size data handle
        unsigned int occ[ASCII_CHAR+1];
        for(int i = 0; i <= ASCII_CHAR; i++){
            occ[i] = 0;
        }
        fseek(bwtfile, 0, SEEK_SET);
        char c = fgetc(bwtfile);
        while(c != EOF){
            occ[cm.ascii[(int) c]]++;
            c = fgetc(bwtfile);
        }
        for(int i = 0; i <= ASCII_CHAR; i++){
            c_table[i] = cumulative;
            cumulative += occ[i];
        }
        
    }

}

struct index_decode_data bwt_decode_backward(FILE *bwtfile, FILE *idxfile, unsigned int c_table[], int file_size, struct char_mapping cm, int start_position){
    struct index_decode_data result;
    int scale = 1;
    int index = 0;

    fseek(bwtfile, start_position, SEEK_SET);
    int c = fgetc(bwtfile);
    unsigned int next = get_occurence(bwtfile, idxfile, c, start_position, cm) + c_table[cm.ascii[(int)c]]-1;
    int flag = 0;
    if (c == ']' && flag == 0){
        flag = 1;
    }else if (c == '[' && flag == 0){
        result.index = -1;
        return result;
    }
    for(int i = 1; i < file_size; i++){
        fseek(bwtfile, next, SEEK_SET);
        c = fgetc(bwtfile);
        if (c == ']' && flag == 0){
            flag = 1;
        }else if (c == '[' && flag == 0){
            result.index = -1;
            return result;
        }else if (c == '[' && flag == 1){
            break;
        }else if (flag == 1 && c >= '0' && c <= '9'){
            index += (c-'0')*scale;
            scale *= 10;
        }
        next = get_occurence(bwtfile, idxfile, c, next, cm) + c_table[cm.ascii[(int)c]]-1;
    }

    result.index = index;
    result.start_position = start_position;

    return result;
}

struct decoded_data bwt_decode(FILE *bwtfile, FILE *idxfile, unsigned int c_table[], int file_size, struct char_mapping cm, int start_position){
    char temp_decoded[MAX_STRING];
    struct decoded_data result;
    int decoded_size = 0;
    int scale = 1;
    int index = 0;

    fseek(bwtfile, start_position, SEEK_SET);
    int c = fgetc(bwtfile);
    unsigned int next = get_occurence(bwtfile, idxfile, c, start_position, cm) + c_table[cm.ascii[(int)c]]-1;
    
    temp_decoded[0] = c;
    int flag = 0;
    if (c == ']' && flag == 0){
        
        flag = 1;
    }
    for(int i = 1; i < file_size; i++){
        fseek(bwtfile, next, SEEK_SET);
        c = fgetc(bwtfile);
        
        temp_decoded[i] = c;
        decoded_size++;
        if (c == ']' && flag == 0){
           // printf("found ] |temp_decoded: %s\n", temp_decoded);
            flag = 1;
        }else if (c == '[' && flag == 0){
           // printf("temp_decoded: %s\n", temp_decoded);
          //  printf("found first [ at position but flag=0: %d\n", next);
            result.index = -1;
            return result;
        }else if (c == '[' && flag == 1){
            break;
        }else if (flag == 1 && c >= '0' && c <= '9'){
            index += (c-'0')*scale;
            scale *= 10;
        }
        next = get_occurence(bwtfile, idxfile, c, next, cm) + c_table[cm.ascii[(int)c]]-1;
    }
    
    next = get_next_bracket(bwtfile, idxfile, cm, c_table,index);

    //Decode backward starts here.
    char temp_decoded_backward[(MAX_STRING-decoded_size)*sizeof(char)];
    int decoded_backward_size = 0;
    fseek(bwtfile, next, SEEK_SET);
    c = fgetc(bwtfile);
    temp_decoded_backward[0] = c;
    
    for(int i = 1; i < file_size; i++){
        if(next == (unsigned int)start_position){
            decoded_backward_size--;
            break;
        }
        next = get_occurence(bwtfile, idxfile, c, next, cm) + c_table[cm.ascii[(int)c]]-1;
        fseek(bwtfile, next, SEEK_SET);
        c = fgetc(bwtfile);
        decoded_backward_size++;
        temp_decoded_backward[decoded_backward_size] = c;
    }

    //construct result
    result.decoded = (char *)malloc((decoded_size+decoded_backward_size+3)*sizeof(char));
    result.decoded_size = decoded_size+decoded_backward_size;
    result.index = index;

    for(int i = 0; i <= decoded_size; i++){
        result.decoded[i] = temp_decoded[decoded_size-i];
    }
    for(int i = 0; i <= decoded_backward_size; i++){
        result.decoded[decoded_size+i+1] = temp_decoded_backward[decoded_backward_size-i];
    }

    result.decoded[decoded_size+decoded_backward_size+2] = '\0';

    return result;
}


int get_next_bracket(FILE *bwtfile, FILE *idxfile, struct char_mapping cm, unsigned int c_table[], int index){
    char c[20]; 
    index++;
    snprintf(c, sizeof(c), "[%d]", index);
    struct search_result result = bwtsearch(bwtfile, idxfile, c_table, c, cm);
    if (result.first > result.last){
        index = 0;
        while (result.first > result.last){
            index++;
            snprintf(c, sizeof(c), "[%d]", index);
            result = bwtsearch(bwtfile, idxfile, c_table, c, cm);
        }
    } 
    
    return result.first;
}

struct search_result bwtsearch(FILE *bwtfile, FILE *idxfile, unsigned int c_table[], char* search_pattern, struct char_mapping cm){
    struct search_result result;
    int search_len = strlen(search_pattern);
    int found = 0;
    char c = search_pattern[search_len-1];
    result.first = c_table[cm.ascii[(int)c]];

    for(int i = cm.ascii[(int)c]; i <= ASCII_CHAR; i++){
        if (c_table[cm.ascii[(int)c]] != c_table[i]){
            found = 1;
            result.last = c_table[i]-1;
            break;
        }
    }
    if (found == 0){
        result.last = c_table[cm.ascii[(int)c]];
    }


    int index = search_len-1;
    while((result.first <= result.last) && index > 0){
        index--;
        c= search_pattern[index];
        result.first = c_table[cm.ascii[(int)c]] + get_occurence(bwtfile, idxfile,c, result.first-1, cm);
        result.last = c_table[cm.ascii[(int)c]] + get_occurence(bwtfile, idxfile,c, result.last, cm)-1;
    }

    return result;
}


void swap(struct index_decode_data* a, struct index_decode_data* b) {
    struct index_decode_data temp = *a;
    *a = *b;
    *b = temp;
}

int partition(struct index_decode_data arr[], int low, int high) {
    int pivot = arr[high].index; // pivot
    int i = (low - 1); // Index of smaller element

    for (int j = low; j <= high - 1; j++) {
        // If current element is smaller than or equal to pivot
        if (arr[j].index <= pivot) {
            i++; // increment index of smaller element
            swap(&arr[i], &arr[j]);
        }
    }
    swap(&arr[i + 1], &arr[high]);
    return (i + 1);
}

// Quick Sort function
void quickSort(struct index_decode_data arr[], int low, int high) {
    if (low < high) {
        // pi is partitioning index, arr[p] is now at right place
        int pi = partition(arr, low, high);

        // Separately sort elements before partition and after partition
        quickSort(arr, low, pi - 1);
        quickSort(arr, pi + 1, high);
    }
}

int binary_search(struct index_decode_data arr[], int l, int r, int x) {
    while (l <= r) {
        int m = l + (r - l) / 2;

        if (arr[m].index == x)
            return m;

        if (arr[m].index < x)
            l = m + 1;

        else
            r = m - 1;
    }

    return -1;
}


int main(int arg, char **argv){
    
    struct char_mapping cm = init_char_mapping();
    int num_block;
    int file_size;

    if(arg < 3){
        printf("Usage: ./bwtsearch <.bwt file> <.idx file> <search term>\n");
        return 1;
    }
    

    FILE *bwtfile = fopen(argv[1], "r");
    if(bwtfile == NULL){
        printf("Error: Cannot open file %s\n", argv[1]);
        return 1;
    }else{
        fseek(bwtfile, 0, SEEK_END);
        file_size = ftell(bwtfile);
        num_block = file_size/BLOCK_SIZE;
        fseek(bwtfile, 0, SEEK_SET);
    }

    FILE *idxfile = fopen(argv[2], "r");
    //if file not exist
    if(idxfile == NULL){
        //create occurence index file
        idxfile = fopen(argv[2], "w");
        create_occurence_index(bwtfile, idxfile, cm, num_block);
        fclose(idxfile);
        idxfile = fopen(argv[2], "r");
    }


    unsigned int c_table[ASCII_CHAR+1];
    create_c_table(num_block, idxfile, bwtfile, c_table,cm);
    
    
    
    struct search_result result_search = bwtsearch(bwtfile, idxfile, c_table, argv[3], cm);
    
    

    
    
    if(result_search.last-result_search.first+1 < 0){
        
        return 0;
    }
    struct index_decode_data result[result_search.last-result_search.first+1];
    struct index_decode_data result_intersect[result_search.last-result_search.first+1];
    int result_size = 0;
    
    
    for (int i = result_search.first; i <= result_search.last; i++){
        struct index_decode_data temp_result = bwt_decode_backward(bwtfile, idxfile, c_table, file_size, cm, i);
        if(temp_result.index != -1){
            result[result_size] = temp_result;
            result_size++;
        }
    }
    


    
    
    //Quick sort result
    quickSort(result, 0, result_size-1);
    

    //remove duplicate
    int j = 0;
    for(int i = 1; i < result_size; i++){
        if(result[j].index != result[i].index){
            j++;
            result[j] = result[i];
        }
    }

    
    result_size = j+1;
    
    for (int i = 1 ; i < arg-3;i++) {
        
        struct search_result result_search_new = bwtsearch(bwtfile, idxfile, c_table, argv[i+3], cm);
        
        int result_intersect_size = 0;
        
        for (int i = result_search_new.first,j=0; i <= result_search_new.last; i++,j++){
            struct index_decode_data temp_result = bwt_decode_backward(bwtfile, idxfile, c_table, file_size, cm, i);
            int binary_search_result = binary_search(result, 0, result_size-1, temp_result.index);
            if (binary_search_result != -1){
                result_intersect[result_intersect_size] = temp_result;
                result_intersect_size++;                
            }
        }
        
        
        //Quick sort result
        quickSort(result_intersect, 0, result_intersect_size-1);

        //remove duplicate
        j = 0;
        for(int i = 1; i < result_intersect_size; i++){
            if(result_intersect[j].index != result_intersect[i].index){
                j++;
                result_intersect[j] = result_intersect[i];
            }
        }
        result_intersect_size = j+1;
        result_size = result_intersect_size;

        for (int i = 0; i < result_size; i++){
            result[i] = result_intersect[i];
        }

        
    }

    
    
    //Decoded result
    //FILE *output = fopen("output.txt", "w");
    for (int i = 0; i < result_size; i++){
        struct decoded_data temp_result = bwt_decode(bwtfile, idxfile, c_table, file_size, cm, result[i].start_position);
        printf("%s\n", temp_result.decoded);
        //fprintf(output, "%s\n", temp_result.decoded);
    }
    
    fclose(bwtfile);
    fclose(idxfile);
}

