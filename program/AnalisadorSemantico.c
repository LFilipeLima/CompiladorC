#include "AnalisadorSemantico.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

TipoSemantico ObterTipo(const char *nome)
{
    if(nome == NULL)
        return tipoIndefinido;

    if(strcmp(nome, "integer") == 0)
        return tipoInteger;

    if(strcmp(nome, "double") == 0)
        return tipoDouble;

    if(strcmp(nome, "char") == 0)
        return tipoChar;

    return tipoIndefinido;
}

const char *NomeTipo(TipoSemantico tipoAtual)
{
    switch(tipoAtual)
    {
        case tipoInteger: return "integer";
        case tipoDouble: return "double";
        case tipoChar: return "char";
        case tipoBooleano: return "booleano";
        default: return "indefinido";
    }
}

void CompararTipos(
    TipoSemantico tipoAtual,
    const Simbolo *simbolo,
    unsigned int linhaAtual,
    const char *funcaoContexto
)
{
    if(tipoAtual == tipoIndefinido || tipoAtual != simbolo->tipo)
    {
        printf(
            "Erro semântico: tipos incompatíveis para '%s': "
            "esperado %s, recebido %s. Linha: %u, função: %s()\n",
            simbolo->nome, NomeTipo(simbolo->tipo), NomeTipo(tipoAtual),
            linhaAtual, funcaoContexto != NULL ? funcaoContexto : "CompararTipos"
        );
        exit(1);
    }
}
