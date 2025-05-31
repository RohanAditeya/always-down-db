#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdbool.h>

typedef enum {
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

typedef enum {
    PREPARE_SUCCESS, 
    PREPARE_UNRECOGNIZED_STATEMENT
} PrepareResult;

typedef enum {
    STATEMENT_INSERT,
    STATEMENT_SELECT
} StatementType;

typedef struct {
    StatementType type;
} Statement;

typedef struct {
    char* buffer;
    int bufferLength;
} InputBuffer;

void printDbPrompt(void);
InputBuffer* createInputBuffer(void);
void readIntoBuffer(InputBuffer* inputBuffer);
void freeInputBuffer(InputBuffer* inputBuffer);
void reallocOrFreeAndExitWithFailure(InputBuffer** ptr, size_t memorySize);
MetaCommandResult doMetaCommand(InputBuffer* inputBuffer);
PrepareResult prepareStatement(InputBuffer* inputBuffer, Statement* statement);
void executeStatement(Statement* statement);
/*
    Main function for now runs a REPL loop that exits when command "exit" is entered.
    Prints "Invalid command" otherwise and waits for next command from the user.
*/
int main(void) {
    InputBuffer* commandBuffer = createInputBuffer();
    while(true) {
        printDbPrompt();
        readIntoBuffer(commandBuffer);
        // check and execute if input is meta command
        if(commandBuffer->buffer[0] == '.') {
            switch(doMetaCommand(commandBuffer)) {
                case META_COMMAND_SUCCESS:
                    continue;
                case META_COMMAND_UNRECOGNIZED_COMMAND:
                    printf("Unrecognized meta command %s\n", commandBuffer->buffer);
                    continue;
            }
        }
        // prepare statement
        Statement* newStatament = malloc(sizeof(Statement));
        switch(prepareStatement(commandBuffer, newStatament)) {
            case PREPARE_SUCCESS:
                break;
            case PREPARE_UNRECOGNIZED_STATEMENT:
                printf("Unrecognized statement %s\n", commandBuffer->buffer);
                free(newStatament);
                continue;
        }
        // execute statement
        executeStatement(newStatament);
        // free statement
        free(newStatament);
    }
}

/*
    Prints the command prompt prefix to indicate the fact that we are waiting
    for user input.
*/
void printDbPrompt(void) {
    printf("Db -> ");
}

/*
    To create a buffer that stores the user input to be evaluated.
    If memory allocation fails the program exits with failure status
*/
InputBuffer* createInputBuffer(void) {
    InputBuffer* newInputBuffer = malloc(sizeof(InputBuffer));
    if(newInputBuffer) {
        newInputBuffer->buffer = NULL;
        newInputBuffer->bufferLength = 0;
        return newInputBuffer;
    } else {
        printf("Cannot allocate memory for input buffer exiting program now\n");
        exit(EXIT_FAILURE);
    }
}

/*
    Method to read the user input entered in the command prompt to the
    buffer passed as argument. The method takes care to dynamically reallocate
    more memory if the buffer array is small to hold the input.
    Method also truncates the buffer is the large for the input.
*/
void readIntoBuffer(InputBuffer* inputBuffer) {
    char ch;
    int inputLength = 0;
    inputBuffer->bufferLength = 10;
    // Intentionally reallocating memory here so that the last allocated memory is not lost.
    inputBuffer->buffer = realloc(inputBuffer->buffer, sizeof(char) * inputBuffer->bufferLength);
    while(ch = getchar(), ch != '\n' && ch != EOF) {
        inputLength++;
        if(inputLength >= inputBuffer->bufferLength) {
            inputBuffer->bufferLength += 10;
            reallocOrFreeAndExitWithFailure(&inputBuffer, sizeof(char) * inputBuffer->bufferLength);
        }
        *(inputBuffer->buffer + inputLength - 1) = ch;
    }
    *(inputBuffer->buffer + inputLength) = '\0';
    if(inputLength < inputBuffer->bufferLength) {
        reallocOrFreeAndExitWithFailure(&inputBuffer, sizeof(char) * inputLength);
        inputBuffer->bufferLength = inputLength;
    }
}

/*
    Deallocate memory allocated before for the buffer
*/
void freeInputBuffer(InputBuffer* inputBuffer) {
    if(inputBuffer->buffer != NULL) {
        printf("Free up memory %p\n", inputBuffer->buffer);
        (inputBuffer->buffer);
    }
    free(inputBuffer);
}

/*
    Method to reallocate new memory block for the buffer with the new size.
    If the allocation fails the program frees up the input buffer and exits with failure status.
*/
void reallocOrFreeAndExitWithFailure(InputBuffer** ptr, size_t memorySize) {
    (*ptr)->buffer = realloc((*ptr)->buffer, memorySize);
    if(*ptr == NULL) {
        freeInputBuffer(*ptr);
        exit(EXIT_FAILURE);
    }
}

/*
    Method to process meta-commands
*/
MetaCommandResult doMetaCommand(InputBuffer* inputBuffer) {
    if(strcmp(inputBuffer->buffer, ".exit") == 0) {
        freeInputBuffer(inputBuffer);
        exit(EXIT_SUCCESS);
    }
    else
        return META_COMMAND_UNRECOGNIZED_COMMAND;
}

/*
    Prepare a statement object for parsing by the virtual machine
*/
PrepareResult prepareStatement(InputBuffer* inputBuffer, Statement* statement) {
    if(strncmp(inputBuffer->buffer, "insert", 6) == 0) {
        statement->type = STATEMENT_INSERT;
        return PREPARE_SUCCESS;
    } else if(strncmp(inputBuffer->buffer, "select", 6) == 0) {
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }
    return PREPARE_UNRECOGNIZED_STATEMENT;
}

/*
    Method to execute the statement
*/
void executeStatement(Statement* statement) {
    switch(statement->type) {
        case STATEMENT_INSERT:
            printf("This is where we do an insert operation\n");
            break;
        case STATEMENT_SELECT:
            printf("This is where we do a select operation\n");
            break;
    }
}