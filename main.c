#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdbool.h>
#include<stdint.h>
#include<inttypes.h>

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255
#define TABLE_MAX_PAGES 100
#define sizeOfAttribute(Struct, attribute) sizeof(((Struct*)0)->attribute)

/*
  ##########################################################################
  These are enums used to denote state/results of any operations part of processing
  the user input
  ##########################################################################
*/

typedef enum {
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

typedef enum {
    PREPARE_SUCCESS, 
    PREPARE_SYNTAX_ERROR,
    PREPARE_UNRECOGNIZED_STATEMENT
} PrepareResult;

typedef enum {
    STATEMENT_INSERT,
    STATEMENT_SELECT
} StatementType;

typedef enum {
    EXECUTE_TABLE_FULL,
    EXECUTE_SUCCESS
} ExecuteResult;

/*
  #################################################################################
  Structures to model data types needed for holding data
  #################################################################################
*/
typedef struct {
    int32_t id;
    char username[COLUMN_USERNAME_SIZE];
    char email[COLUMN_EMAIL_SIZE];
} Row;

typedef struct {
    int numberOfRows;
    void* pages[TABLE_MAX_PAGES];
} Table;

typedef struct {
    StatementType type;
    Row* insertRow;
} Statement;

typedef struct {
    char* buffer;
    int bufferLength;
} InputBuffer;

const uint32_t ID_COLUMN_SIZE = sizeOfAttribute(Row, id);
const uint32_t USERNAME_COLUMN_SIZE = sizeOfAttribute(Row, username);
const uint32_t EMAIL_COLUMN_SIZE = sizeOfAttribute(Row, email);
const uint32_t ROW_SIZE = ID_COLUMN_SIZE + USERNAME_COLUMN_SIZE + EMAIL_COLUMN_SIZE;
const uint32_t PAGE_SIZE = 4096;
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const uint32_t ID_COLUMN_OFFSET = 0;
const uint32_t USERNAME_COLUMN_OFFSET = ID_COLUMN_OFFSET + ID_COLUMN_SIZE;
const uint32_t EMAIL_COLUMN_OFFSET = USERNAME_COLUMN_OFFSET + USERNAME_COLUMN_SIZE;
const uint32_t TABLE_MAX_ROWS = 5;

void printDbPrompt(void);
InputBuffer* createInputBuffer(void);
void readIntoBuffer(InputBuffer* inputBuffer);
void freeInputBuffer(InputBuffer* inputBuffer);
void reallocOrFreeAndExitWithFailure(InputBuffer** ptr, size_t memorySize);
MetaCommandResult doMetaCommand(InputBuffer* inputBuffer, Table* table);
PrepareResult prepareStatement(InputBuffer* inputBuffer, Statement* statement);
ExecuteResult executeStatement(Statement* statement, Table* table);
void freeStatementPointer(Statement* statement);
ExecuteResult executeInsertStatement(Statement* statement, Table* table);
ExecuteResult executeSelectStatement(Statement* statement, Table* table);
void* findRowSlot(Table* table, uint32_t rowNum);
void serializeAndStoreRow(Row* rowToInsert, void* destinationInPage);
void deserializeAndFetchRow(Row* row, void* destinationInPage);
void freeTable(Table* table);
/*
    Main function for now runs a REPL loop that exits when command "exit" is entered.
    Prints "Invalid command" otherwise and waits for next command from the user.
*/
int main(void) {
    InputBuffer* commandBuffer = createInputBuffer();
    Table* table = malloc(sizeof(Table));
    table->numberOfRows = 0;
    for(int i = 0; i < TABLE_MAX_PAGES; i++)
        table->pages[i] = NULL;
    while(true) {
        printDbPrompt();
        readIntoBuffer(commandBuffer);
        // check and execute if input is meta command
        if(commandBuffer->buffer[0] == '.') {
            switch(doMetaCommand(commandBuffer, table)) {
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
            case PREPARE_SYNTAX_ERROR:
                printf("Failed to parse statement with syntax error\n");
                freeStatementPointer(newStatament);
                continue;
            case PREPARE_UNRECOGNIZED_STATEMENT:
                printf("Unrecognized statement %s\n", commandBuffer->buffer);
                freeStatementPointer(newStatament);
                continue;
        }
        // execute statement
        switch(executeStatement(newStatament, table)) {
            case EXECUTE_SUCCESS:
                printf("Executed statement\n");
                break;
            case EXECUTE_TABLE_FULL:
                printf("Error. table full\n");
                break;
        }
        // free statement
        freeStatementPointer(newStatament);
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
    if(inputBuffer->buffer != NULL) 
        free(inputBuffer->buffer);
    free(inputBuffer);
}

/*
    Method to clear all allocated blocks in statement
*/
void freeStatementPointer(Statement* statement) {
    free(statement->insertRow);
    free(statement);
}

/*
    Method to clear all allocated blocks in table
*/
void freeTable(Table* table) {
    for (int i = 0; i < TABLE_MAX_PAGES; i++)
        free(table->pages[i]);
    free(table);
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
MetaCommandResult doMetaCommand(InputBuffer* inputBuffer, Table* table) {
    if(strcmp(inputBuffer->buffer, ".exit") == 0) {
        freeInputBuffer(inputBuffer);
        freeTable(table);
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
        // parse statement to find id, username, email
        Row* insertRow = malloc(sizeof(Row));
        int capturedArgumentCount = sscanf(inputBuffer->buffer, "insert %"SCNi32" %32s %255s", &(insertRow->id), insertRow->username, insertRow->email);
        // assign them to statement insertRow field 
        statement->insertRow = insertRow;
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
ExecuteResult executeStatement(Statement* statement, Table* table) {
    switch(statement->type) {
        case STATEMENT_INSERT:
            return executeInsertStatement(statement, table);
            break;
        case STATEMENT_SELECT:
            return executeSelectStatement(statement, table);
            break;
    }
}

/*
    method to handle execution of insert statement
    the method writes to an in-memory array of pages
    pages are a collection of many rows and the maximum number of rows in a
    page is controlled by ROWS_PER_PAGE constant
    A Page is just a block of memory allocated to store many rows. Each row is appendend to
    the last page 
*/
ExecuteResult executeInsertStatement(Statement* statement, Table* table) {
    if(table->numberOfRows >= TABLE_MAX_ROWS)
        return EXECUTE_TABLE_FULL;
    serializeAndStoreRow(statement->insertRow, findRowSlot(table, table->numberOfRows));
    table->numberOfRows++;
    return EXECUTE_SUCCESS;
}

/*
    method to handle execution of select statement
    currently the method just prints all records in the table
*/
ExecuteResult executeSelectStatement(Statement* statement, Table* table) {
    Row* row = malloc(sizeof(Row));
    for(int i = 0; i < table->numberOfRows; i++) {
        deserializeAndFetchRow(row, findRowSlot(table, i));
        printf("Fetched row with ID: %"PRId32" USERNAME: %s EMAIL: %s\n", row->id, row->username, row->email);
    }
    free(row);
    return EXECUTE_SUCCESS;
}

/*
    Method to calculate the write/read location in memory for a row in the table
    using the rowNum.
    The location is found by locating the page number the rowNum belongs to
    and the offset from the base address for the page to locate the row.
*/
void* findRowSlot(Table* table, uint32_t rowNum) {
    int pageNumber = rowNum / ROWS_PER_PAGE;
    void* page = table->pages[pageNumber];
    if(page == NULL)
        page = table->pages[pageNumber] = malloc(PAGE_SIZE);
    int rowOffset = rowNum % ROWS_PER_PAGE;
    int byteOffset = rowOffset * ROW_SIZE;
    return page + byteOffset;
}

/*
    method to copy the row data into the page memory block
*/
void serializeAndStoreRow(Row* rowToInsert, void* destinationInPage) {
    memcpy(destinationInPage + ID_COLUMN_OFFSET, &(rowToInsert->id), ID_COLUMN_SIZE);
    memcpy(destinationInPage + USERNAME_COLUMN_OFFSET, rowToInsert->username, USERNAME_COLUMN_SIZE);
    memcpy(destinationInPage + EMAIL_COLUMN_OFFSET, rowToInsert->email, EMAIL_COLUMN_SIZE);
}

/*
    method to copy the row data from the page memory block
*/
void deserializeAndFetchRow(Row* row, void* destinationInPage) {
    memcpy(&(row->id), destinationInPage + ID_COLUMN_OFFSET, ID_COLUMN_SIZE);
    memcpy(row->username, destinationInPage + USERNAME_COLUMN_OFFSET, USERNAME_COLUMN_SIZE);
    memcpy(row->email, destinationInPage + EMAIL_COLUMN_OFFSET, EMAIL_COLUMN_SIZE);
}