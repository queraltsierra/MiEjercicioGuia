#include <stdio.h>
#include <string.h>
#include <stdlib.h>


int main(int argc, char *argv[]) 

{
		char peticion[100];
		char resultado[100];
		strcpy(peticion,"Miguel/47/Biel/19/Maria/22/Juan/12"); //resultat: Miguel*47-Biel*19-Maria*22
		char nombre[20];
		int edat;
		char *p = strtok(peticion,"/");
		while (p!=0)
		{
			strcpy(nombre,p);
			p = strtok (NULL,"/");
			edat = atoi(p);
			if (edat > 18)
				sprintf(resultado,"%s%s*%d-",resultado,nombre,edat);
			
			p = strtok(NULL,"/");
		}
		resultado[strlen(resultado)-1] = '\0';
	printf("Resultat: %s\n", resultado);
}