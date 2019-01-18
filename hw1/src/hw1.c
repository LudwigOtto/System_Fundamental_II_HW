#include "hw1.h"
#include <stdlib.h>
#ifdef _STRING_H
#error "Do not #include <string.h>. You will get a ZERO."
#endif

#ifdef _STRINGS_H
#error "Do not #include <strings.h>. You will get a ZERO."
#endif

#ifdef _CTYPE_H
#error "Do not #include <ctype.h>. You will get a ZERO."
#endif

/**
 * @brief Validates command line arguments passed to the program.
 * @details This function will validate all the arguments passed to the program
 * and will return a unsigned short (2 bytes) that will contain the
 * information necessary for the proper execution of the program.
 *
 * IF -p is given but no (-r) ROWS or (-c) COLUMNS are specified this function
 * MUST set the lower bits to the default value of 10. If one or the other
 * (rows/columns) is specified then you MUST keep that value rather than assigning the default.
 *
 * @param argc The number of arguments passed to the program from the CLI.
 * @param argv The argument strings passed to the program from the CLI.
 * @return Refer to homework document for the return value of this function.
 */

static int validateKey(int pfFlag) {
	const char *srcAlphabetTable;
    char *tarAlphabetTable;
	int ptr = 0;
	int endOfTarTable;
	int lengthOfAlphabets;
	if(pfFlag == 0) {
		srcAlphabetTable = polybius_alphabet;
		tarAlphabetTable = polybius_table;
		lengthOfAlphabets = LENGTH_POLY_ALPHABET;
		endOfTarTable = 256;
	}
	else {
		srcAlphabetTable = fm_alphabet;
		tarAlphabetTable = fm_key;
		lengthOfAlphabets = LENGTH_FM_ALPHABET;
		endOfTarTable = 26;
	}
	if(key == NULL) return EXIT_FAILURE;
	while(*(key + ptr) != '\0') {
		char val = *(key + ptr) - *(srcAlphabetTable);
		if(val >= 0 && val < lengthOfAlphabets) {
			for(int i=0; i<ptr;i++){
				if(*(tarAlphabetTable + i) == *(key + ptr))
					return EXIT_FAILURE;
			}
			*(tarAlphabetTable + ptr) = *(key + ptr);
			ptr++;
		}
		else
			return EXIT_FAILURE;
	}
	*(tarAlphabetTable + endOfTarTable) = (char)ptr;
	return EXIT_SUCCESS;
}

static int validateRowAndCol(unsigned short row, unsigned short col) {
	if(row < 16 && row > 8 && col < 16 && col > 8) {
		if(row*col > LENGTH_POLY_ALPHABET)
			return EXIT_SUCCESS;
		else
			return EXIT_FAILURE;
	}
	return EXIT_FAILURE;
}


unsigned short validargs(int argc, char **argv) {
	if(argc > 1)
	{
		int i;
		unsigned short usageCode = 0x0000;
		unsigned short row = 10;
		unsigned short col = 10;
		int pfFlag = -1;
		/*
		* Let pfFlag denote Polybius (pfFlag is 0) or Fractionated Morse (pfFlag is 1) method.
		* if undefined, pfFlag is -1
		* */
		int cryptFlag = -1;
		/*
		* Let cryptFlag denote Encryption (cryptFlag is 0) or Decryption (cryptFlag is 1).
		* if undefined, cryptFlag is -1
		* */
		for(i=1;i<argc;i++)
		{
			char arg = *(*(argv+i)+1);
			if(arg == 'h')
				return HELP_CODE;
			switch(arg) {
				case 'f':
					if(pfFlag == 0) return FAULT_CODE;
					usageCode |= FMC_CODE;
					pfFlag = 1;
					break;
				case 'p':
					if(pfFlag == 1) return FAULT_CODE;
					pfFlag = 0;
					usageCode |= DFLTRC_CODE;
					break;
				case 'e':
					if(pfFlag == -1 || cryptFlag != -1) return FAULT_CODE;
					cryptFlag = 0;
					break;
				case 'd':
					if(pfFlag == -1 || cryptFlag != -1) return FAULT_CODE;
					usageCode |= DCRPT_CODE;
					cryptFlag = 1;
					break;
				case 'k':
					if(pfFlag == -1 || cryptFlag == -1) return FAULT_CODE;
					i++;
					key = *(argv+i);
					if(validateKey(pfFlag)) return FAULT_CODE;
					break;
				case 'r':
					if(pfFlag != 0 || cryptFlag == -1) return FAULT_CODE;
					i++;
					row = (unsigned short)atoi(*(argv+i));
					break;
				case 'c':
					if(pfFlag != 0 || cryptFlag == -1) return FAULT_CODE;
					i++;
					col = (unsigned short)atoi(*(argv+i));
					break;
				default:
					goto final;
			}
		}
		if(!validateRowAndCol(row, col)) {
			if(row != 10) {
				usageCode |= ROW_CODE;
				usageCode &= ((row << INIT_ROW_BIT) | ~ROW_CODE);
			}
			if(col != 10) {
				usageCode |= COL_CODE;
				usageCode &= ((col << INIT_COL_BIT) | ~COL_CODE);
			}
		}
		else
			return FAULT_CODE;
		return usageCode;
	}
final:
	return FAULT_CODE;
}

static char* initCipherTable(unsigned short pfFlag) {
	char *tarTable;
	const char* srcTable;
	int endOfTarTable;
	int lengthOfAlphabets;
	if(pfFlag == 0) {
		tarTable = polybius_table;
		srcTable = polybius_alphabet;
		endOfTarTable = 256;
		lengthOfAlphabets = LENGTH_POLY_ALPHABET;
	}
	else {
		tarTable = fm_key;
		srcTable = fm_alphabet;
		endOfTarTable = 26;
		lengthOfAlphabets = LENGTH_FM_ALPHABET;
	}
	int consecutivePtr = (int)*(tarTable + endOfTarTable);
	int j = 0;
	for(int i=0;i < lengthOfAlphabets; i++) {
		int ptr = 0;
		int checkRepeat = 0;
		while(ptr < consecutivePtr) {
			if(*(srcTable + i) == *(tarTable + ptr)) {
				checkRepeat = 1;
				break;
			}
			else
				ptr++;
		}
		if(!checkRepeat) {
			*(tarTable + consecutivePtr + j) = *(srcTable + i);
			j++;
		}
	}
	return tarTable;
}

static int inputRow = -1;
static int inputCol = -1;

static int doPolybiusCipher(unsigned short cryptFlag, unsigned short customRow,
		unsigned short customCol, char input) {
	if(cryptFlag == 0) {
		if(input != ' ') {
			for(int i=0;i < LENGTH_POLY_ALPHABET; i++) {
				if(input == *(polybius_table + i)) {
					printf("%X%X",i/(int)customCol, i%(int)customCol);
					break;
				}
			}
		}
		else {
			printf("%c", input);
		}
	}
	else {
		if(input != ' ') {
			if(inputRow == -1) {
				inputRow = strtol(&input, NULL, 16);
				if(inputRow < 0 || inputRow > 15)
					return EXIT_FAILURE;
			}
			else {
				inputCol = strtol(&input, NULL, 16);
				if(inputCol < 0 || inputRow > 15)
					return EXIT_FAILURE;
			}
			if(inputRow != -1 && inputCol != -1) {
				int ptr = (inputRow * customCol) + inputCol;
				char output;
				if(ptr < LENGTH_POLY_ALPHABET)
					output = *(polybius_table + ptr);
				else
					output = '\0';
				printf("%c", output);
				inputRow = -1;
				inputCol = -1;
			}
		}
		else {
			printf("%c", input);
		}
	}
	return EXIT_SUCCESS;
}

static char codeInPosit0, codeInPosit1, codeInPosit2;
static int curPosit = 0;
static int checkAlreadySpace = 0;
static short antiTranslateCode = 0;
static int xIsDetected = 0;
static int lengthOfCode = 0;

static int giveCodeToPosit(const char* src) {
	int threePositsFull = 0;
	switch(curPosit) {
		case 0:
			codeInPosit0 = *src;
			curPosit = 1;
			break;
		case 1:
			codeInPosit1 = *src;
			curPosit = 2;
			break;
		case 2:
			codeInPosit2 = *src;
			curPosit = 0;
			threePositsFull = 1;
			break;
		}
	return threePositsFull;
}

static inline int isSpaceAhead() {
	if(codeInPosit0 == '\0' &&
		codeInPosit1 == '\0' &&
		codeInPosit2 == '\0' &&
		curPosit == 0)
		return 1;
	else
		return 0;
}

static inline int indexOfCode(char code) {
	if(code == '.') return 0;
	else if(code == '-') return 1;
	else return 2;
}

static void doMorseEncryptAndPrint() {
	int index0 = indexOfCode(codeInPosit0);
	int index1 = indexOfCode(codeInPosit1);
	int index2 = indexOfCode(codeInPosit2);
	int ptr = index0 * 9 + index1 * 3 + index2;
	if(ptr < LENGTH_FM_ALPHABET)
		printf("%c",*(fm_key + ptr));
}

static void doAntiTranslateMorse() {
	int i, j;
	for(i=0;i < LENGTH_POLY_ALPHABET;i++) {
		for(j=0;j < lengthOfCode;j++) {
			char decryptedCode = (char)(antiTranslateCode >> (lengthOfCode - j - 1));
			decryptedCode &= 0x01;
			decryptedCode = (decryptedCode == 0x01) ? '-' : '.';
			if(*(morse_table + i) + j == NULL ||
				*(*(morse_table + i) + j) != decryptedCode)
				break;
		}
		if(j == lengthOfCode) {
			if(*(morse_table + i) + j != NULL && *(*(morse_table + i) + j) == '\0') {
				break;
			}
		}
	}
	printf("%c", *(polybius_alphabet + i));
	antiTranslateCode = 0;
	lengthOfCode = 0;
}

static int doFMCipher(unsigned short cryptFlag, char input) {
	const char* translateCode;
	char endOfLeter = 'x';
	if(cryptFlag == 0) {
		int checkThreePosits;
		if(input != ' ') {
			checkAlreadySpace = 0;
			for(int i=0;i < LENGTH_POLY_ALPHABET; i++) {
				if(input == *(polybius_alphabet + i)) {
					translateCode = *(morse_table + i);
					break;
				}
			}
			if(*translateCode == '\0')
				return EXIT_FAILURE;
			int j = 0;
			while(*(translateCode+j) != '\0') {
				checkThreePosits = giveCodeToPosit((translateCode + j));
				if(checkThreePosits) doMorseEncryptAndPrint();
				j++;
			}
			checkThreePosits = giveCodeToPosit(&endOfLeter);
			if(checkThreePosits) doMorseEncryptAndPrint();
		}
		else {
			if(isSpaceAhead())
				checkThreePosits = giveCodeToPosit(&endOfLeter);
			if(!checkAlreadySpace) {
				checkThreePosits = giveCodeToPosit(&endOfLeter);
				if(checkThreePosits) doMorseEncryptAndPrint();
				checkAlreadySpace = 1;
			}
		}
	}
	else
	{
		if(input == ' ' || input < 65 || input > 90)
			return EXIT_FAILURE;
		int i;
		for(i=0;i < LENGTH_FM_ALPHABET; i++) {
			if(input == *(fm_key + i)) {
				break;
			}
		}
		const char *decryptedCode= *(fractionated_table + i);
		int j = 0;
		while(*(decryptedCode + j) != '\0') {
			if(*(decryptedCode + j) == 'x') {
				if(xIsDetected && lengthOfCode == 0) {
					printf(" ");
					xIsDetected = 0;
				}
				else {
					doAntiTranslateMorse();
					xIsDetected = 1;
				}
			}
			else {
				char morseLetter = (*(decryptedCode + j) == '-') ? 0x01 : 0x00;
				antiTranslateCode = (antiTranslateCode << 1) + (short)morseLetter;
				lengthOfCode++;
				xIsDetected = 0;
			}
			j++;
		}
	}

	return EXIT_SUCCESS;
}

int activateCrypto(unsigned short mode) {
	unsigned short pfFlag = mode & FMC_CODE;
	unsigned short cryptFlag = mode & DCRPT_CODE;
	unsigned short row = (mode & ROW_CODE) >> INIT_ROW_BIT;
	unsigned short col = (mode & COL_CODE) >> INIT_COL_BIT;

	char *table = initCipherTable(pfFlag);
	if(table == NULL)
		return EXIT_FAILURE;
	char input = 0;
	while((input = (char)fgetc(stdin)) &&  input != EOF) {
		if(input == '\n') {
			if(cryptFlag == DCRPT_CODE) {
				if(lengthOfCode > 0)
				{
					char endOfLine = 'Z';
					if(doFMCipher(cryptFlag, endOfLine))
						return EXIT_FAILURE;
					/*
					 * Since the anti-translation is triggered by detecting the 'x',
					 * leverage morse code of letter 'Z' (i.e. "xx-") to activate
					 * the anti-translation for the residual codes in the end of line.
					 */
				}
				//Initialize the buffer to read the input code
				inputRow = -1;
				inputCol = -1;
				antiTranslateCode = 0;
				xIsDetected = 0;
				lengthOfCode = 0;
			}
			else {
				if(curPosit == 2 && codeInPosit1 == 'x') {
					if(doFMCipher(cryptFlag, ' '))
						return EXIT_FAILURE;
				}
				/*
				 * If there are residual codes not be encrypted yet, use the space to
				 * add 'x' and trigger the encryption.
				 */
				codeInPosit0 = '\0';
				codeInPosit1 = '\0';
				codeInPosit2 = '\0';
				curPosit = 0;
				checkAlreadySpace = 0;
			}
			printf("\n");
		}
		else {
			if(pfFlag == 0) {
				if(doPolybiusCipher(cryptFlag, row, col, input))
					return EXIT_FAILURE;
			}
			else {
				if(doFMCipher(cryptFlag, input))
					return EXIT_FAILURE;
			}
		}
	}
	return EXIT_SUCCESS;
}
