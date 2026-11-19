#ifndef ANALISADOR_SEMANTICO_H
#define ANALISADOR_SEMANTICO_H

#include "TabelaSimbolos.h"

TipoSemantico ObterTipo(const char *nome);
const char *NomeTipo(TipoSemantico tipoAtual);
void CompararTipos(
    TipoSemantico tipoAtual,
    const Simbolo *simbolo,
    unsigned int linhaAtual,
    const char *funcaoContexto
);

#endif
