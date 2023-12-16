#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct queue_element * head = NULL;
struct queue_element * tail = NULL;

struct pte ** root;

//stats that are recorded and output during program execution
int curr_open_frame, num_frames, num_mem_accesses, num_page_faults, num_writes, num_leaves;

//used to determine what eviction scheme, and what trace file to read from
char * trace_file_name, *scheme;

struct cmd {
	char op;
	
	int next_use;
	int last_use;

	unsigned int root_index;
	unsigned int leaf_index;

};

//used to store preprocessed entries from the trace file
struct queue_element {
	int queue_index;
	struct cmd * instruct;
	struct queue_element *next;
};

//used to store pte's in the page table
struct pte {
	int root_index;
	int next_use;
	int last_use;
	int frame_num;

	bool refrenced;
	bool valid;
	bool modified;
};

int pwr(int base, int exp){
	int result = 1, i;
	for(i = 0; i < exp; i ++){
		result = result * base;
	}
	return result;
}

//used in address translation
unsigned int hex_to_dec(char * hex){
	int i, val;
	int dec = 0;
	int len = strlen(hex) - 1;

	for(i = 0; hex[i] != '\0'; i ++){
		if(hex[i] >= '0' && hex[i] <= '9'){
			val = hex[i] - 48;
		}
		else if (hex[i] >= 'a' && hex[i] <= 'f'){
			val = (hex[i] - 97) +10;
		}
		else if (hex[i] >= 'A' && hex[i] <= 'F'){
			val = (hex[i] - 65) + 10;
		}
		dec = dec + (val * pwr(16, len));
		len --;
	}

	return dec;
}

//used to add instructions to a queue, helps optimize preformance
int enqueue(struct cmd * instruct){
	struct queue_element * new_element = malloc(sizeof(struct queue_element));
	
	if(new_element == NULL){
		printf("ERROR : Couldn't allocate space for new queue element...\n");
		return 1;
	}

	else{
		//regardless we must give the new element it's defined members//
		new_element->instruct = instruct;

		//queue is empty
		if(head == NULL){
			head = new_element;
			tail = new_element;
		}

		//queue isn't empty
		else{
			tail->next = new_element;
			tail = new_element;
		}
	}
	return 0;
}

int cmp_instruct(struct cmd * inst1, struct cmd * inst2){
	if(inst1->root_index == inst2->root_index && inst1->leaf_index == inst2->leaf_index){
		return 1;
	}
	else{
		return 0;
	}
}

void mark_next_ref(){
	int curr_index = 0;
	struct queue_element * curr_element = head;
	struct queue_element * search_element;

	struct cmd * curr_instruct = curr_element->instruct;
	struct cmd * search_instruct;
	
	tail->instruct->next_use = -1; //	V- was put here because this -V

	while(curr_element != tail){ //<-don't think this would reach the tail element?//
		search_element = curr_element->next;
		search_instruct = search_element->instruct;

		curr_instruct->next_use = -1; 

		while(search_element != tail){

			if(cmp_instruct(search_instruct, curr_instruct)){
				curr_instruct->next_use = search_element->queue_index - curr_index;
				break;
			}

			search_element = search_element->next;
			search_instruct = search_element->instruct;
		}

		//advances both the instruction and the queue element
		curr_element = curr_element->next;
		curr_instruct = curr_element->instruct;

		curr_index ++;
	}
}

int init_instruct_queue(){
	int queue_index = 0;
	//opening trace file, ensuring it worked properly//
	FILE * trace_fptr = fopen(trace_file_name, "r");
	if(trace_fptr == NULL){
		printf("ERROR : Couldn't open the trace file ~%s~\n", trace_file_name);
		exit(-1);
	}
	else{
		char data_buf [100];
		//reads file, creates new instruction struct for each memory
		while(fgets(data_buf, sizeof(data_buf), trace_fptr)){
			//used to ignore those pesky first few lines of ouput//
			if(data_buf[0] != '='){
				struct cmd * new_instruct = (struct cmd *) malloc(sizeof(struct cmd));
				
				char * token = strtok(data_buf, " ");
				char op = token[0];

				new_instruct->op = token[0];

				token = strtok(NULL, " ");
				token = strtok(token, ",");	
				
				//converts the string address to a number we can work with
				unsigned int address = hex_to_dec(token);

				token = strtok(NULL, " ");
									
				char size = token[0];
				
				new_instruct->root_index = address >> 23;
				new_instruct->leaf_index = (address >> 13) & 1023;

				//add new element to the queue
				struct queue_element * new_element = malloc(sizeof(struct queue_element));
				
				if(new_element == NULL){
					printf("ERROR : Couldn't allocate space for new queue element...\n");
					exit(-1);
				}

				else{
					//regardless we must give the new element it's defined members//
					new_element->instruct = new_instruct;
					new_element->queue_index = queue_index;

					//queue is empty
					if(head == NULL){
						head = new_element;
						tail = new_element;
					}

					//queue isn't empty
					else{
						tail->next = new_element;
						tail = new_element;
					}
				}
				queue_index ++;
			}
		}

		mark_next_ref();
		fclose(trace_fptr);
		return 0;
	}
}

void swap(struct pte ** valid_ptes, int index1, int index2)
{
    struct pte * temp = valid_ptes[index1];
    valid_ptes[index1] = valid_ptes[index2];
    valid_ptes[index2] = temp;
}

void sort_ptes(struct pte ** valid_ptes){
    int i, j;
    bool swapped;
    
	for (i = 0; i < num_frames - 1; i++) {
        swapped = false;
        for (j = 0; j < num_frames - 1 - i; j++) {
            if (valid_ptes[j]->last_use > valid_ptes[j + 1]->last_use) {
                swap(valid_ptes, j, j + 1);
                swapped = true;
            }
        }
        if (swapped == false){
			break;
		}
    }
}

struct pte * get_frame(int now){
	int frame = -1, i, x, index = 0;

	struct pte * valid_ptes[num_frames];

	struct pte * curr_leaf, * curr_pte;

	//iterate through all root entries
	for(i = 0; i < 512; i ++){
		//if the leaf has a valid entry
		if(root[i] != NULL){
			//iterate through all leaf entries
			for(x = 0; x < 1024; x ++){
				curr_pte = &root[i][x];
				//if the current PTE within the leaf is valid, copy its address into the valid_pte's array
				if(curr_pte->valid == true){
					valid_ptes[index] = curr_pte;
					index ++;
				}
			}
		}
	}

	switch(scheme[0]){
		//opt eviction
		case 'o':
			{
				struct pte * curr_pte, * selected_pte;
				int highest = -999999999;
				
				for(i = 0; i < num_frames; i ++){
					curr_pte = valid_ptes[i];

					//optimally we want to evict the page not used again
					if(curr_pte->next_use == -1){
						return curr_pte;
					}
					//otherwise we need to find the page used farthest in the future
					else{
						if((curr_pte->next_use) > highest){
							highest = curr_pte->next_use;
							selected_pte = curr_pte;
						}
					}
				}
				return selected_pte;
			}
		//lru
		case 'l':
			{
				struct pte * curr_pte, * selected_pte;
				int lowest = 999999999, i;
				for(i = 0; i < num_frames; i++){
					curr_pte = valid_ptes[i];
					if(curr_pte->last_use < lowest){
						selected_pte = valid_ptes[i];
					}
				}
				return selected_pte;
				break;
			}
		//clock
		case 'c':
			{
				x = 0;
				sort_ptes(valid_ptes);
				while(1){
					struct pte * curr_pte = valid_ptes[x % num_frames];
					if(curr_pte->refrenced == false){
						return curr_pte;
					}
					else{
						curr_pte->refrenced = false;
					}
					x ++;
				}
				return NULL;
				break;
			}
	
	}
	return NULL;
}

int simulate(){
	int i, x, now = 0;
	struct pte * curr_leaf, * evicted_pte;
	struct queue_element * curr_element = head, * temp;
	struct cmd * curr_instruct;
	
	for(i = 0; i < 512; i ++){
		root[i] = NULL;
	}

	while(curr_element != NULL){
		curr_instruct = curr_element->instruct;
		curr_leaf = root[curr_instruct->root_index];

		//there is no current 2nd level mapping of this virtual address, page fault
		if(curr_leaf == NULL){
			num_page_faults ++;
			//allocate new memory for leaf
			struct pte * new_leaf = (struct pte *)malloc(sizeof(struct pte) * 1024);

			//makes sure new leaf has been allocated properly
			if(new_leaf == NULL){
				printf("ERROR : Couldn't allocate space for new leaf\n");
				exit(-1);
			}
			//mark new leaf's PTE's as invalid and such
			for(i = 0; i < 1024; i ++){
				//valid pte being marked
				if(i == curr_instruct->leaf_index){
					new_leaf[i].valid = true;

					//if the pte is being modified
					if(curr_instruct->op == 'M'){
						num_mem_accesses ++;
						new_leaf[i].modified = true;
					}

					//if the pte is being stored to
					else if(curr_instruct->op == 'S'){
						new_leaf[i].modified = true;
					}
					
					//just fetching an instruction, page has not been modified yet
					else{
						num_mem_accesses ++;
						new_leaf[i].modified = false;
					}

					new_leaf[i].next_use = curr_instruct->next_use;
					new_leaf[i].last_use = now;
					new_leaf[i].root_index = curr_instruct->root_index;
					new_leaf[i].refrenced = true;

					//there's open frames to use
					if(curr_open_frame > 0){
						new_leaf[i].frame_num = curr_open_frame;
						curr_open_frame --;
					}

					//no open frames, must evict
					else{
						//finds the pte that should be evicted based on whatever scheme was requested
						evicted_pte = get_frame(now);

						if(evicted_pte->modified == true){
							num_writes ++;
						}

						//update the new pte's frame number to the evicted frames number
						new_leaf[i].frame_num = evicted_pte->frame_num;

						//mark evicted pte as invalid and trim
						evicted_pte->valid = false;
						bool trim = true;

						for(x = 0; x < 1024; x++){
							//found a valid leaf entry, don't trim
							if(root[evicted_pte->root_index][x].valid == true){
								trim = false;
								break;
							}
						}

						//we must trim, there were no valid pte's in the leaf
						//free any memory that was allocated to the leaf and mark it as NULL
						if(trim){
							int evicted_root_index = evicted_pte->root_index;
							free(root[evicted_root_index]);
							root[evicted_root_index] = NULL;
						}
					}
				}
				//invalid pte being marked
				else{
					//may only need to mark it as invalid, but just in case I update all the proper variables
					new_leaf[i].valid = false;
					new_leaf[i].modified = false;
					new_leaf[i].refrenced = false;
					new_leaf[i].next_use = curr_instruct->next_use;
					new_leaf[i].last_use = now;
					new_leaf[i].root_index = curr_instruct->root_index;
				}
			}
			//at this point the new leaf has been properly created, we just need to mark it in the page table
			root[curr_instruct->root_index] = new_leaf;
		}
		
		//the instruction is trying to fetch from a valid leaf
		else{

			//grabs a refrence to the pte trying to be accessed
			struct pte * ref_pte = &root[curr_instruct->root_index][curr_instruct->leaf_index];

			//page is not loaded, get a frame
			if(ref_pte->valid == false){
				ref_pte->refrenced = true;
				//There's open frames to be used
				if(curr_open_frame > 0){
					ref_pte->valid = true;
					ref_pte->frame_num = curr_open_frame;
					curr_open_frame --;
				}
				
				//No more open frames, must evict
				else{
					evicted_pte = get_frame(now);

					if(evicted_pte->modified == true){
						num_writes ++;
					}

					ref_pte->frame_num = evicted_pte->frame_num;
					ref_pte->valid = true;

					//mark evicted pte as invalid and trim
					evicted_pte->valid = false;
					bool trim = true;

					for(x = 0; x < 1024; x++){
						//found a valid leaf entry, don't trim
						if(root[evicted_pte->root_index][x].valid == true){
							trim = false;
							break;
						}
					}

					//we must trim, there were no valid pte's in the leaf
					//free any memory that was allocated to the leaf and mark it as NULL
					if(trim){
						free(root[evicted_pte->root_index]);
						root[evicted_pte->root_index] = NULL;
					}
				}
			}

			ref_pte->next_use = curr_instruct->next_use;
			ref_pte->last_use = now;
			//if the pte is being modified
			if(curr_instruct->op == 'M'){
				num_mem_accesses ++;
				ref_pte->modified = true;
			}

			//if the pte is being stored to
			else if(curr_instruct->op == 'S'){
				ref_pte->modified = true;
			}
			
			//just fetching an instruction, page has not been modified yet
			else{
				num_mem_accesses ++;
				ref_pte->modified = false;
			}

			ref_pte->refrenced = true;
		}

		//removes the element from the queue
		temp = curr_element->next;

		free(curr_instruct);
		free(curr_element);

		//advances to the next instruction
		curr_element = temp;
		now ++;
	}
	return 0;
}

//--------------------------------------BEGIN MAIN-------------------------------------//
int main(int argc, char * argv[]){
	//ensures correct argument count//
	if(argc != 6){
		printf("Incorrect number of arguments, expected 6, got %d...\n", argc);
		return 1;
	}

	else{
		//converts command line args into usable variables
		num_frames = atoi(argv[2]);
		scheme = argv[4];
		trace_file_name = argv[5];
		curr_open_frame = num_frames;
		
		printf("Initializing instruction queue...\n");
		//initializes the instruction queue, automatically marks each instruction with the next use
		init_instruct_queue();
		printf("Finished initializing instruction queue...\n");

		//initializes the page table
		root = malloc(sizeof(struct pte *) * 512);
		if(root == NULL){
			printf("ERROR : Failed to allocate space for page table\n");
			return 1;
		}

		printf("Beginning simulation\n");
		simulate();

		//outputs recorded statistics to a file "stats.txt"
		FILE * output_fptr = fopen("stats.txt", "w");

		if(output_fptr == NULL){
			printf("ERROR : Couldn't open output file...\n");
			return 1;
		}

		//opened file correctly
		else{
			//finds the number of leaves
			int i;
			for(i = 0; i < 512; i ++){
				if(root[i] != NULL){
					num_leaves ++;
				}
			}

			int page_table_size = 2048 + (num_leaves * 4096);
			fprintf(output_fptr, "Algorithm : %s\nNumber of frames : %d\nTotal memory accesses : %d\nTotal page faults : %d\nTotal writes to disk : %d\nNumber of page table leaves : %d\nTotal size of page table : %d\n", scheme, num_frames, num_mem_accesses, num_page_faults, num_writes, num_leaves, page_table_size);
	}
	return 0;
}
