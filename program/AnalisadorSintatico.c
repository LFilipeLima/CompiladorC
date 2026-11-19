#include "basics.h"
#include "AnalisadorLexico.h"
#include "AnalisadorSemantico.h"
#include "GeradorCodigo.h"
#include "TabelaSimbolos.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/*
    Integrantes
    Nomes   RAs
    Daniel  24123
    Luis    24139
*/

Token token;
TabelaSimbolos tabela;

// Assinaturas de métodos caso ocorra recursão indireta.
void verificaBloco();
void VerificaComando();
TipoSemantico VerificaExpressao();

static int indiceRotinaAtual = -1;
static int profundidadeBloco = 0;
static bool codigoPrincipalIniciado = false;
static char rotuloPrincipal[32];

static void CriarRotuloRotina(const Simbolo *simbolo, char *destino, size_t tamanho)
{
    snprintf(destino, tamanho, "SUB_%lu", simbolo->identificadorCodigo);
}

static void CriarRotuloFonte(const Simbolo *simbolo, char *destino, size_t tamanho)
{
    snprintf(destino, tamanho, "SRC_%lu", simbolo->identificadorCodigo);
}

static int ContarPosicoesDoEscopo(int escopo, bool incluirParametrosPorValor)
{
    int quantidade = 0;
    for(int i = 0; i < tabela.tamanhoLogico; i++)
    {
        Simbolo *simbolo = &tabela.tabela[i];
        if(simbolo->escopo == escopo && simbolo->token == variavel &&
           simbolo->modoPassagem != MODO_REFERENCIA &&
           (incluirParametrosPorValor ||
            simbolo->modoPassagem == MODO_NAO_APLICAVEL))
            quantidade++;
    }
    return quantidade;
}

static bool RotuloExpressao(const Simbolo *simbolo)
{
    return simbolo != NULL && simbolo->token == funcao &&
           simbolo->escopoCorpo >= 0 && simbolo->deslocamento != -1 &&
           tabela.ScopoAtual >= simbolo->escopoCorpo;
}

static void EmitirComparacao(Token operador)
{
    switch(operador)
    {
        case igual: CMIG(); break;
        case diferente: CMDG(); break;
        case menor: CMME(); break;
        case menorouigual: CMEG(); break;
        case maior: CMMA(); break;
        case maiorouigual: CMAG(); break;
        default: break;
    }
}

static long ValorCaractere(const char *literal)
{
    if(literal[1] != '\\')
        return (unsigned char)literal[1];

    switch(literal[2])
    {
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case '0': return '\0';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'v': return '\v';
        default: return (unsigned char)literal[2];
    }
}


static Simbolo *BuscarRotuloVisivel(char *nome)
{
    for(int i = tabela.tamanhoLogico - 1; i >= 0; i--)
    {
        if(tabela.tabela[i].token == rotulo &&
           strcmp(tabela.tabela[i].nome, nome) == 0)
        {
            return &tabela.tabela[i];
        }
    }
    return NULL;
}

static void InserirSimboloOuErro(Simbolo simbolo, bool aumentarEscopo)
{
    if(!InserirSimbolo(&tabela, simbolo, aumentarEscopo))
    {
        printf(
            "Erro: identificador '%s' já declarado neste escopo. "
            "Linha: %u, função: %s()\n",
            simbolo.nome, linha, __func__
        );
        exit(1);
    }
}

bool verificaType(Token token)
{
    return token == identificador && ObterTipo(palavraAtual) != tipoIndefinido;
}

static void EmitirChamada(Simbolo *simbolo);

static void EmitirInicioRotina(int indice)
{
    Simbolo *rotina = &tabela.tabela[indice];
    char rotulo[40];
    char corpo[40];
    CriarRotuloRotina(rotina, rotulo, sizeof(rotulo));
    snprintf(corpo, sizeof(corpo), "BODY_%lu", rotina->identificadorCodigo);
    NADA(rotulo);
    ENPR(rotina->escopoCorpo);
    DSVR(corpo);
}

static void EmitirCorpoRotina(int indice)
{
    Simbolo *rotina = &tabela.tabela[indice];
    char corpo[40];
    snprintf(corpo, sizeof(corpo), "BODY_%lu", rotina->identificadorCodigo);
    NADA(corpo);
    AMEM(ContarPosicoesDoEscopo(rotina->escopoCorpo, true));
    int quantidadeValor = 0;
    for(int i = 0; i < rotina->quantidadeParametros; i++)
        quantidadeValor += rotina->parametros[i].modoPassagem == MODO_VALOR;
    if(quantidadeValor > 0)
        IPVL(rotina->escopoCorpo, quantidadeValor);
}

static void EmitirChamada(Simbolo *simbolo)
{
    bool leitura = simbolo->intrinseco && strcmp(simbolo->nome, "read") == 0;
    bool escrita = simbolo->intrinseco && strcmp(simbolo->nome, "write") == 0;
    int indiceParametro = 0;
    if(simbolo->token == funcao)
        AMEM(1);
    token = Analex();

    while(token != fechaparenteses)
    {
        if(token == finalDeArquivo)
        {
            printf("Erro: Esperava-se um fechaparenteses. Linha: %u, função: %s()\n",
                   linha, __func__);
            exit(-1);
        }

        if(leitura)
        {
            if(token != identificador)
            {
                printf("Erro: read espera identificadores. Linha: %u, função: %s()\n",
                       linha, __func__);
                exit(-1);
            }
            Simbolo *destino = BuscarSimbolo(&tabela, palavraAtual);
            if(destino == NULL || destino->token != variavel)
            {
                printf("Erro: identificador '%s' não é uma variável legível. Linha: %u\n",
                       palavraAtual, linha);
                exit(-1);
            }
            LEIT();
            int nivel = destino->escopo;
            if(destino->modoPassagem == MODO_REFERENCIA)
                ARMI(nivel, destino->deslocamento);
            else
                ARMZ(nivel, destino->deslocamento);
            token = Analex();
        }
        else if(!escrita && indiceParametro < simbolo->quantidadeParametros &&
                simbolo->parametros[indiceParametro].modoPassagem == MODO_REFERENCIA)
        {
            if(token != identificador)
            {
                printf("Erro semântico: parâmetro por referência exige variável. Linha: %u\n",
                       linha);
                exit(1);
            }
            char nome[sizeof(palavraAtual)];
            strcpy(nome, palavraAtual);
            Simbolo *atual = BuscarSimbolo(&tabela, nome);
            if(atual == NULL || atual->token != variavel ||
               atual->tipo != simbolo->parametros[indiceParametro].tipo)
            {
                printf("Erro semântico: argumento incompatível para '%s'. Linha: %u\n",
                       simbolo->nome, linha);
                exit(1);
            }
            EmitirReferenciaSimbolo(nome, 1);
            token = Analex();
        }
        else
        {
            TipoSemantico tipoArgumento = VerificaExpressao();
            if(!escrita && indiceParametro < simbolo->quantidadeParametros &&
               tipoArgumento != simbolo->parametros[indiceParametro].tipo)
            {
                printf("Erro semântico: argumento incompatível para '%s'. Linha: %u\n",
                       simbolo->nome, linha);
                exit(1);
            }
            if(escrita)
                IMPR();
        }

        indiceParametro++;
        if(token == virgula)
            token = Analex();
        else if(token != fechaparenteses)
        {
            printf("Erro: Esperava-se uma vírgula ou fechaparenteses. Linha: %u, função: %s()\n",
                   linha, __func__);
            exit(-1);
        }
    }

    if(!leitura && !escrita && indiceParametro != simbolo->quantidadeParametros)
    {
        printf("Erro semântico: quantidade de parâmetros incompatível para '%s'. Linha: %u\n",
               simbolo->nome, linha);
        exit(1);
    }

    token = Analex();
    if(!leitura && !escrita)
    {
        char rotulo[40];
        CriarRotuloRotina(simbolo, rotulo, sizeof(rotulo));
        CHPR(rotulo, simbolo->escopoCorpo);
    }
}

TipoSemantico Fator()
{
    if(token == numero)
    {
        CRCT(strtol(palavraAtual, NULL, 10));
        token = Analex();
        return tipoInteger;
    }

    if(token == caractere)
    {
        CRCT(ValorCaractere(palavraAtual));
        token = Analex();
        return tipoChar;
    }

    if(token == nao)
    {
        token = Analex();
        Fator();
        NEGA();
        return tipoBooleano;
    }

    if(token == abreparenteses)
    {
        token = Analex();
        TipoSemantico tipoAtual = VerificaExpressao();

        if(token != fechaparenteses)
        {
            printf(
                "Erro: Esperava-se um fechaparenteses. "
                "Linha: %u, função: %s()\n",
                linha, __func__
            );
            exit(-1);
        }

        token = Analex();
        return tipoAtual;
    }

    if(token == identificador)
    {
        Simbolo *simbolo = BuscarSimbolo(&tabela,palavraAtual);

        if(simbolo == NULL)
        {
            printf(
                "Erro: identificador '%s' não declarado. "
                "Linha: %u, função: %s()\n",
                palavraAtual, linha, __func__
            );
            exit(-1);
        }

        token = Analex();

        if(token == abreparenteses)
        {
            if(simbolo->token != funcao)
            {
                printf(
                    "Erro: identificador '%s' não é uma função. "
                    "Linha: %u, função: %s()\n",
                    simbolo->nome, linha, __func__
                );
                exit(-1);
            }

            EmitirChamada(simbolo);
            return simbolo->tipo;
        }

        if(token == abrecolchetes)
        {
            if(simbolo->token != variavel)
            {
                printf(
                    "Erro: identificador '%s' não é uma variável. "
                    "Linha: %u, função: %s()\n",
                    simbolo->nome, linha, __func__
                );
                exit(-1);
            }

            printf("Erro semântico: acesso indexado não é suportado pelo gerador MEPA. Linha: %u\n",
                   linha);
            exit(1);

            token = Analex();
            VerificaExpressao();

            while(token == virgula)
            {
                token = Analex();
                VerificaExpressao();
            }

            if(token != fechacolchetes)
            {
                printf(
                    "Erro: Esperava-se um fechacolchetes. "
                    "Linha: %u, função: %s()\n",
                    linha, __func__
                );
                exit(-1);
            }

            token = Analex();
            return simbolo->tipo;
        }

        if(simbolo->token == procedimento ||
           simbolo->token == rotulo ||
           simbolo->token == programa)
        {
            printf(
                "Erro: identificador '%s' não pode ser usado em uma expressão. "
                "Linha: %u, função: %s()\n",
                simbolo->nome, linha, __func__
            );
            exit(-1);
        }

        if(simbolo->token == variavel)
            EmitirReferenciaSimbolo(simbolo->nome, 0);
        else if(RotuloExpressao(simbolo))
        {
            CRVL(simbolo->escopoCorpo, simbolo->deslocamento);
        }
        return simbolo->tipo;
    }

    printf(
        "Erro: Esperava-se um identificador, número, caractere, abreparenteses ou nao. "
        "Linha: %u, função: %s()\n",
        linha, __func__
    );
    exit(-1);
}

TipoSemantico Termo()
{
    if(token != identificador &&
       token != numero &&
       token != caractere &&
       token != nao &&
       token != abreparenteses)
    {
        printf(
            "Erro: Esperava-se um fator. Linha: %u, função: %s()\n",
            linha, __func__
        );
        exit(-1);
    }

    TipoSemantico tipoAtual = Fator();

    while(token == vezes || token == dividir || token == e)
    {
        Token operador = token;
        token = Analex();

        if(token != identificador &&
           token != numero &&
           token != caractere &&
           token != nao &&
           token != abreparenteses)
        {
            printf(
                "Erro: Esperava-se um fator. Linha: %u, função: %s()\n",
                linha, __func__
            );
            exit(-1);
        }

        TipoSemantico tipoFator = Fator();
        if(operador == e)
        {
            CONJ();
            tipoAtual = tipoBooleano;
        }
        else if(tipoAtual != tipoFator)
            tipoAtual = tipoIndefinido;
        if(operador == vezes) MULT();
        else if(operador == dividir) DIVI();
    }

    return tipoAtual;
}

TipoSemantico ExpressaoSimples()
{
    Token sinal = token;
    if(token == mais || token == menos)
    {
        token = Analex();
    }

    if(token != identificador &&
       token != numero &&
       token != caractere &&
       token != nao &&
       token != abreparenteses)
    {
        printf(
            "Erro: Esperava-se um Termo. Linha: %u, função: %s()\n",
            linha, __func__
        );
        exit(-1);
    }

    TipoSemantico tipoAtual = Termo();
    if(sinal == menos) INVR();

    while(token == mais || token == menos || token == ou)
    {
        Token operador = token;
        token = Analex();

        if(token != identificador &&
           token != numero &&
           token != caractere &&
           token != nao &&
           token != abreparenteses)
        {
            printf(
                "Erro: Esperava-se um Termo. Linha: %u, função: %s()\n",
                linha, __func__
            );
            exit(-1);
        }

        TipoSemantico tipoTermo = Termo();
        if(operador == ou)
        {
            DISJ();
            tipoAtual = tipoBooleano;
        }
        else if(tipoAtual != tipoTermo)
            tipoAtual = tipoIndefinido;
        if(operador == mais) SOMA();
        else if(operador == menos) SUBT();
    }

    return tipoAtual;
}

static bool EhOperadorRelacional(Token token)
{
    return token == igual ||
           token == diferente ||
           token == menor ||
           token == menorouigual ||
           token == maior ||
           token == maiorouigual;
}

TipoSemantico VerificaExpressao()
{
    TipoSemantico tipoAtual = ExpressaoSimples();

    if(EhOperadorRelacional(token))
    {
        Token operador = token;
        token = Analex();
        ExpressaoSimples();
        EmitirComparacao(operador);
        return tipoBooleano;
    }

    return tipoAtual;
}


void VerificaCondicao()
{
    ExpressaoSimples();

    if(!EhOperadorRelacional(token))
    {
        printf(
            "Erro: esperava-se um operador relacional na condição. "
            "Linha: %u, função: %s()\n",
            linha, __func__
        );
        exit(-1);
    }

    Token operador = token;
    token = Analex();
    ExpressaoSimples();
    EmitirComparacao(operador);
}

void VerificaComandoSemRotulo()
{
    if(token == identificador)
    {
        Simbolo *simbolo = BuscarSimbolo(&tabela,palavraAtual);

        if(simbolo == NULL)
        {
            printf(
                "Erro: identificador '%s' não declarado. "
                "Linha: %u, função: %s()\n",
                palavraAtual, linha, __func__
            );
            exit(-1);
        }
        Simbolo *simboloAtual = BuscarSimbolo(&tabela,palavraAtual);
        token = Analex();
     
        if(token == abrecolchetes)
        {
            if(simbolo->token != variavel)
            {
                printf(
                    "Erro: identificador '%s' não é uma variável. "
                    "Linha: %u, função: %s()\n",
                    simbolo->nome, linha, __func__
                );
                exit(-1);
            }

            printf("Erro semântico: atribuição indexada não é suportada pelo gerador MEPA. Linha: %u\n",
                   linha);
            exit(1);
            
            token = Analex();
           
            VerificaExpressao();
                if(EhOperadorRelacional(token)){
                printf("Erro: não usar operador relacional em atribuição"
                "linha: %u, função %s()\n",linha,__func__);
                exit(-1);
            }
        
            while(token == virgula)
            {
                token = Analex();
                VerificaExpressao();
            }

            if(token != fechacolchetes)
            {
                printf(
                    "Erro: Esperava-se um fechacolchetes. "
                    "Linha: %u, função: %s()\n",
                    linha, __func__
                );
                exit(-1);
            }

            token = Analex();
        }
      
        if(token == atribuicao)
        {
           
            if(simbolo->token != variavel && simbolo->token != funcao)
            {
                printf(
                    "Erro: identificador '%s' não pode receber atribuição. "
                    "Linha: %u, função: %s()\n",
                    simbolo->nome, linha, __func__
                );
                exit(-1);
            }

            token = Analex();
            
          
            TipoSemantico tipoAtual = ExpressaoSimples();
            if(EhOperadorRelacional(token)){
                printf("Erro: não usar operador relacional em atribuição"
                "linha: %u, função %s()\n",linha,__func__);
                exit(-1);
            }

            CompararTipos(tipoAtual, simboloAtual, linha, "CompararTipos");
            //atualizar
            Simbolo atualizado = *simboloAtual;
            //atualizado.valor = palavraAtual;
            AtualizarSimbolo(&tabela, atualizado);
            if(simboloAtual->token == funcao && RotuloExpressao(simboloAtual))
            {
                ARMZ(simboloAtual->escopoCorpo, simboloAtual->deslocamento);
            }
            else if(simbolo->modoPassagem == MODO_REFERENCIA)
                ARMI(simbolo->escopo, simbolo->deslocamento);
            else
                ARMZ(simbolo->escopo, simbolo->deslocamento);
        }
        else if(token == abreparenteses)
        {
            if(simbolo->token != procedimento)
            {
                printf(
                    "Erro: identificador '%s' não é um procedimento. "
                    "Linha: %u, função: %s()\n",
                    simbolo->nome, linha, __func__
                );
                exit(-1);
            }
            EmitirChamada(simbolo);
        }
        else if(simbolo->token == procedimento)
        {
            if(simbolo->quantidadeParametros != 0)
            {
                printf("Erro semântico: procedimento '%s' exige parâmetros. Linha: %u\n",
                       simbolo->nome, linha);
                exit(1);
            }
            char rotulo[40];
            CriarRotuloRotina(simbolo, rotulo, sizeof(rotulo));
            CHPR(rotulo, simbolo->escopoCorpo);
        }
    }
    else if(token == vapara)
    {
        token = Analex();

        if(token != numero)
        {
            printf(
                "Erro: Esperava-se um número. Linha: %u, função: %s()\n",
                linha, __func__
            );
            exit(-1);
        }

        if(BuscarRotuloVisivel(palavraAtual) == NULL)
        {
            printf(
                "Erro: rótulo '%s' não declarado. "
                "Linha: %u, função: %s()\n",
                palavraAtual, linha, __func__
            );
            exit(-1);
        }

        Simbolo *destino = BuscarRotuloVisivel(palavraAtual);
        char rotulo[40];
        CriarRotuloFonte(destino, rotulo, sizeof(rotulo));
        DSVR(rotulo);
        token = Analex();
    }
    else if(token == inicio)
    {
        profundidadeBloco++;
        VerificaComando();

        while(token != fim)
        {
            if(token != pontoevirgula)
            {
                printf(
                    
                    "token: %s"
                    "Erro: Esperava-se um ponto e virgula. "
                    "Linha: %u, função: %s()\n",
                    tokenString[token],linha, __func__
                );
                exit(-1);
            }

            VerificaComando();
        }

        token = Analex();
        profundidadeBloco--;
        if(profundidadeBloco == 0 && indiceRotinaAtual >= 0)
        {
            Simbolo *rotina = &tabela.tabela[indiceRotinaAtual];
            DMEM(ContarPosicoesDoEscopo(rotina->escopoCorpo, true));
            RTPR(rotina->escopoCorpo, rotina->quantidadeParametros);
        }
    }
    else if(token == se)
    {
        token = Analex();
        VerificaCondicao();

        unsigned long identificadorRotulo = GerarRotulo();
        char rotuloSenao[32], rotuloFim[32];
        snprintf(rotuloSenao, sizeof(rotuloSenao), "L%lu", identificadorRotulo);
        snprintf(rotuloFim, sizeof(rotuloFim), "L%lu", GerarRotulo());

        if(token != entao)
        {
            printf(
                "Erro: Esperava-se um entao. Linha: %u, função: %s()\n",
                linha, __func__
            );
            exit(-1);
        }

        token = Analex();
        DSVF(rotuloSenao);
        VerificaComandoSemRotulo();

        if(token == senao)
        {
            DSVR(rotuloFim);
            NADA(rotuloSenao);
            token = Analex();
            VerificaComandoSemRotulo();
            NADA(rotuloFim);
        }
        else
            NADA(rotuloSenao);
    }
    else if(token == enquanto)
    {
        char rotuloInicio[32], rotuloFim[32];
        snprintf(rotuloInicio, sizeof(rotuloInicio), "L%lu", GerarRotulo());
        snprintf(rotuloFim, sizeof(rotuloFim), "L%lu", GerarRotulo());
        NADA(rotuloInicio);
        token = Analex();
        VerificaCondicao();

        if(token != faca)
        {
            printf(
                "Erro: Esperava-se um faca. Linha: %u, função: %s()\n",
                linha, __func__
            );
            exit(-1);
        }

        token = Analex();
        DSVF(rotuloFim);
        VerificaComandoSemRotulo();
        DSVR(rotuloInicio);
        NADA(rotuloFim);
    }
    else
    {
        printf(
            "Erro: Esperava-se um identificador, vapara, inicio, se ou enquanto. "
            "Linha: %u, função: %s()\n",
            linha, __func__
        );
        exit(-1);
    }
}

void VerificaComando()
{
    token = Analex();

    if(token == fim)
    {
        return;
    }

    if(token != numero &&
       token != identificador &&
       token != vapara &&
       token != inicio &&
       token != se &&
       token != enquanto)
    {
        printf(
            "Erro: Esperava-se um número ou comando sem rótulo. "
            "Linha: %u, função: %s()\n",
            linha, __func__
        );
        exit(-1);
    }

    if(token == numero)
    {
        Simbolo *simboloRotulo = BuscarRotuloVisivel(palavraAtual);
        if(simboloRotulo == NULL)
        {
            printf(
                "Erro: rótulo '%s' não declarado. "
                "Linha: %u, função: %s()\n",
                palavraAtual, linha, __func__
            );
            exit(-1);
        }

        char rotulo[40];
        CriarRotuloFonte(simboloRotulo, rotulo, sizeof(rotulo));
        NADA(rotulo);

        token = Analex();

        if(token != doispontos)
        {
            printf(
                "Erro: Esperava-se dois pontos após rótulo. "
                "Linha: %u, função: %s()\n",
                linha, __func__
            );
            exit(-1);
        }

        token = Analex();
    }

    VerificaComandoSemRotulo();
}

bool EhBloco()
{
    bool Ehrotulo = token == rotulo;
    bool EhTipo = token == tipo || verificaType(token);
    bool EhImplicito = token == variavel;
    bool EhProcedimento = token == procedimento;
    bool EhFuncao = token == funcao;
    bool EhBegin = token == inicio;

    return Ehrotulo ||
           EhTipo ||
           EhImplicito ||
           EhProcedimento ||
           EhFuncao ||
           EhBegin;
}

void parametrosFormais()
{
    while(token == pontoevirgula || token == abreparenteses)
    {
        token = Analex();

        if(token != identificador &&
           token != variavel &&
           token != funcao &&
           token != procedimento)
        {
            printf(
                "Erro: Esperava-se um identificador, variavel, funcao ou procedimento. "
                "Linha: %u, função: %s()\n",
                linha, __func__
            );
            exit(-1);
        }

        ModoPassagem modoFormal = MODO_VALOR;
        if(token == variavel)
        {
            modoFormal = MODO_REFERENCIA;
            token = Analex();

            if(token != identificador)
            {
                printf(
                    "Erro: Esperava-se um identificador. "
                    "Linha: %u, função: %s()\n",
                    linha, __func__
                );
                exit(-1);
            }
        }

        if(token == identificador)
        {
            int primeiro = tabela.tamanhoLogico;

            Simbolo simboloFormal = GerarSimbolo(
                palavraAtual, variavel, &tabela, NULL
            );
            simboloFormal.modoPassagem = modoFormal;
            InserirSimboloOuErro(simboloFormal, false);

            token = Analex();

            if(token != doispontos)
            {
                printf(
                    "Erro: Esperava-se um dois pontos. "
                    "Linha: %u, função: %s()\n",
                    linha, __func__
                );
                exit(-1);
            }

            token = Analex();

            if(verificaType(token) == false)
            {
                printf(
                    "Erro: Esperava-se um tipo. Linha: %u, função: %s()\n",
                    linha, __func__
                );
                exit(-1);
            }

            tabela.tabela[primeiro].valor = malloc(strlen(palavraAtual) + 1);

            if(tabela.tabela[primeiro].valor == NULL)
                exit(-1);

            strcpy(tabela.tabela[primeiro].valor, palavraAtual);
            tabela.tabela[primeiro].tipo = ObterTipo(palavraAtual);
            if(indiceRotinaAtual >= 0 && !RegistrarParametroFormal(
                &tabela.tabela[indiceRotinaAtual],
                tabela.tabela[primeiro].tipo,
                modoFormal,
                tabela.tabela[primeiro].deslocamento
            ))
                exit(EXIT_FAILURE);
            token = Analex();

            while(token == virgula)
            {
                token = Analex();

                if(token != identificador)
                {
                    printf(
                        "Erro: Esperava-se um identificador. "
                        "Linha: %u, função: %s()\n",
                        linha, __func__
                    );
                    exit(-1);
                }

                Simbolo simboloFormal = GerarSimbolo(
                    palavraAtual,
                    variavel,
                    &tabela,
                    tabela.tabela[primeiro].valor
                );
                simboloFormal.modoPassagem = modoFormal;
                InserirSimboloOuErro(simboloFormal, false);
                tabela.tabela[tabela.tamanhoLogico - 1].tipo =
                    tabela.tabela[primeiro].tipo;
                if(indiceRotinaAtual >= 0 && !RegistrarParametroFormal(
                    &tabela.tabela[indiceRotinaAtual],
                    tabela.tabela[primeiro].tipo,
                    modoFormal,
                    tabela.tabela[tabela.tamanhoLogico - 1].deslocamento
                ))
                    exit(EXIT_FAILURE);

                token = Analex();
            }
        }

        if(token == funcao)
        {
            int primeiro = tabela.tamanhoLogico;
            token = Analex();

            if(token != identificador)
            {
                printf(
                    "Erro: Esperava-se um identificador. "
                    "Linha: %u, função: %s()\n",
                    linha, __func__
                );
                exit(-1);
            }

            InserirSimboloOuErro(
                GerarSimbolo(palavraAtual, funcao, &tabela, NULL)
            , false);

            token = Analex();

            while(token == virgula)
            {
                token = Analex();

                if(token != identificador)
                {
                    printf(
                        "Erro: Esperava-se um identificador. "
                        "Linha: %u, função: %s()\n",
                        linha, __func__
                    );
                    exit(-1);
                }

                InserirSimboloOuErro(
                    GerarSimbolo(palavraAtual, funcao, &tabela, NULL),
                    false
                );

                token = Analex();
            }

            if(token != doispontos)
            {
                printf(
                    "Erro: Esperava-se um dois pontos. "
                    "Linha: %u, função: %s()\n",
                    linha, __func__
                );
                exit(-1);
            }

            token = Analex();

            if(verificaType(token) == false)
            {
                printf(
                    "Erro: Esperava-se um tipo de retorno. "
                    "Linha: %u, função: %s()\n",
                    linha, __func__
                );
                exit(-1);
            }

            for(int i = primeiro; i < tabela.tamanhoLogico; i++)
            {
                tabela.tabela[i].valor = malloc(strlen(palavraAtual) + 1);

                if(tabela.tabela[i].valor == NULL)
                    exit(-1);

                strcpy(tabela.tabela[i].valor, palavraAtual);
                tabela.tabela[i].tipo = ObterTipo(palavraAtual);
            }

            token = Analex();
        }

        if(token == procedimento)
        {
            token = Analex();

            if(token != identificador)
            {
                printf(
                    "Erro: Esperava-se um identificador. "
                    "Linha: %u, função: %s()\n",
                    linha, __func__
                );
                exit(-1);
            }

            InserirSimboloOuErro(
                GerarSimbolo(palavraAtual, procedimento, &tabela, NULL)
            , false);

            token = Analex();

            while(token == virgula)
            {
                token = Analex();

                if(token != identificador)
                {
                    printf(
                        "Erro: Esperava-se um identificador. "
                        "Linha: %u, função: %s()\n",
                        linha, __func__
                    );
                    exit(-1);
                }

                InserirSimboloOuErro(
                    GerarSimbolo(palavraAtual, procedimento, &tabela, NULL),
                    false
                );

                token = Analex();
            }
        }
    }

    if(token != fechaparenteses)
    {
        printf(
            "Erro: Esperava-se um fechaparenteses. "
            "Linha: %u, função: %s()\n",
            linha, __func__
        );
        printf("Token encontrado: %s\n", tokenString[token]);
        exit(-1);
    }
}

void AtribuirParametosFormais()
{
    parametrosFormais();
    token = Analex();
}

static void FinalizarAssinaturaRotina(int indiceRotina)
{
    Simbolo *rotina = &tabela.tabela[indiceRotina];
    bool temValor = false;
    bool temReferencia = false;
    int indiceFormal = 0;

    for(int i = 0; i < tabela.tamanhoLogico; i++)
    {
        Simbolo *formal = &tabela.tabela[i];
        if(formal->escopo != rotina->escopoCorpo)
            continue;
        if(formal->token == funcao || formal->token == procedimento)
        {
            fprintf(stderr,
                "Erro semântico: parâmetros de rotina como argumento ainda não são gerados. Linha: %u\n",
                linha);
            exit(EXIT_FAILURE);
        }
        if(formal->token != variavel || formal->modoPassagem == MODO_NAO_APLICAVEL)
            continue;

        temValor |= formal->modoPassagem == MODO_VALOR;
        temReferencia |= formal->modoPassagem == MODO_REFERENCIA;
        if(formal->modoPassagem == MODO_REFERENCIA)
            /* Argumentos ficam negativos em relação a D[k], abaixo de retorno e display. */
            formal->deslocamento = -(rotina->quantidadeParametros + 2 - indiceFormal);
        rotina->parametros[indiceFormal].deslocamento = formal->deslocamento;
        indiceFormal++;
    }

    if(temValor && temReferencia)
    {
        fprintf(stderr,
            "Erro semântico: a convenção IPVL do projeto não representa modos mistos na mesma rotina. Linha: %u\n",
            linha);
        exit(EXIT_FAILURE);
    }

    if(rotina->token == funcao)
    {
        /* A chamada reserva o resultado antes dos argumentos; D[k] fica n+3 posições acima. */
        rotina->deslocamento = -(rotina->quantidadeParametros + 3);
    }
}

void AtribuirFuncao()
{
    token = Analex();

    if(token != identificador)
    {
        printf(
            "Erro: esperava-se um identificador. "
            "Linha: %u, função: %s()\n",
            linha, __func__
        );
        exit(1);
    }

    int indiceFuncao = tabela.tamanhoLogico;
    int indiceRotinaAnterior = indiceRotinaAtual;
    Simbolo simbFunc = GerarSimbolo(palavraAtual, funcao, &tabela, NULL);

    InserirSimboloOuErro(simbFunc, true);
    tabela.tabela[indiceFuncao].identificadorCodigo = GerarRotulo();
    indiceRotinaAtual = indiceFuncao;
    tabela.tabela[indiceFuncao].escopoCorpo = tabela.ScopoAtual;
    token = Analex();

    if(token != abreparenteses && token != doispontos)
    {
        printf(
            "Erro: esperava-se um parametro formal ou um dois pontos. "
            "Linha: %u, função: %s()\n",
            linha, __func__
        );
        printf("Token encontrado: %s\n", tokenString[token]);
        exit(1);
    }

    if(token == abreparenteses)
    {
        AtribuirParametosFormais();
    }

    FinalizarAssinaturaRotina(indiceFuncao);

    if(token != doispontos)
    {
        printf(
            "Erro: esperava-se dois pontos (tipo de retorno). "
            "Linha: %u, função: %s()\n",
            linha, __func__
        );
        exit(1);
    }

    token = Analex();

    if(verificaType(token) == false)
    {
        printf(
            "Erro: esperava-se um tipo de retorno. "
            "Linha: %u, função: %s()\n",
            linha, __func__
        );
        exit(1);
    }

    tabela.tabela[indiceFuncao].valor = malloc(strlen(palavraAtual) + 1);

    if(tabela.tabela[indiceFuncao].valor == NULL)
        exit(-1);

    strcpy(tabela.tabela[indiceFuncao].valor, palavraAtual);
    tabela.tabela[indiceFuncao].tipo = ObterTipo(palavraAtual);
    token = Analex();

    if(token != pontoevirgula)
    {
        printf(
            "Erro: esperava-se um ponto e virgula. "
            "Linha: %u, função: %s()\n",
            linha, __func__
        );
        exit(1);
    }

    token = Analex();

    if(!EhBloco())
    {
        printf(
            "Erro: esperava-se um bloco. Linha: %u, função: %s()\n",
            linha, __func__
        );
        printf("Token encontrado: %s\n", tokenString[token]);
        exit(1);
    }

    EmitirInicioRotina(indiceFuncao);
    verificaBloco();

    if(token != pontoevirgula)
    {
        printf(
            "Erro: esperava-se um ponto e virgula após o bloco da função. "
            "Linha: %u, função: %s()\n",
            linha, __func__
        );
        printf("Token encontrado: %s\n", tokenString[token]);
        exit(1);
    }

    token = Analex();
    RemoverScopo(&tabela, tabela.ScopoAtual);
    indiceRotinaAtual = indiceRotinaAnterior;
}

void AtribuirProcedimento()
{
    token = Analex();

    if(token != identificador)
    {
        printf(
            "Erro: esperava-se um identificador. "
            "Linha: %u, função: %s()\n",
            linha, __func__
        );
        exit(1);
    }

    int indiceRotinaAnterior = indiceRotinaAtual;
    int indiceProcedimento = tabela.tamanhoLogico;
    Simbolo simbProc = GerarSimbolo(
        palavraAtual, procedimento, &tabela, NULL
    );

    InserirSimboloOuErro(simbProc, true);
    tabela.tabela[indiceProcedimento].identificadorCodigo = GerarRotulo();
    indiceRotinaAtual = indiceProcedimento;
    tabela.tabela[indiceProcedimento].escopoCorpo = tabela.ScopoAtual;
    token = Analex();

    if(token == abreparenteses)
    {
        AtribuirParametosFormais();
    }

    FinalizarAssinaturaRotina(indiceProcedimento);

    if(token != pontoevirgula)
    {
        printf(
            "Erro: esperava-se um ponto e virgula. "
            "Linha: %u, função: %s()\n",
            linha, __func__
        );
        printf("Token encontrado: %s\n", tokenString[token]);
        exit(1);
    }

    token = Analex();

    if(!EhBloco())
    {
        printf(
            "Erro: esperava-se um bloco. Linha: %u, função: %s()\n",
            linha, __func__
        );
        printf("Token encontrado: %s\n", tokenString[token]);
        exit(1);
    }

    EmitirInicioRotina(indiceProcedimento);
    verificaBloco();

    if(token != pontoevirgula)
    {
        printf(
            "Erro: esperava-se um ponto e virgula. "
            "Linha: %u, função: %s()\n",
            linha, __func__
        );
        printf("Token encontrado: %s\n", tokenString[token]);
        exit(-1);
    }

    token = Analex();
    RemoverScopo(&tabela, tabela.ScopoAtual);
    indiceRotinaAtual = indiceRotinaAnterior;
}

void AtribuirTipoImplicito()
{
    if(token != variavel)
    {
        printf(
            "Erro: esperava-se a uma variavel. Linha: %u, função: %s()\n",
            linha, __func__
        );
        printf("Token encontrado: %s\n", tokenString[token]);
        exit(-1);
    }

    token = Analex();

    if(token != identificador)
    {
        printf(
            "Erro: esperava-se um identificador. "
            "Linha: %u, função: %s()\n",
            linha, __func__
        );
        printf("Token encontrado: %s\n", tokenString[token]);
        exit(-1);
    }

    int primeiro = tabela.tamanhoLogico;

    InserirSimboloOuErro(
        GerarSimbolo(palavraAtual, variavel, &tabela, NULL),
        false
    );

    token = Analex();

    while(token == virgula)
    {
        token = Analex();

        if(token != identificador)
        {
            printf(
                "Erro: esperava-se um identificador. "
                "Linha: %u, função: %s()\n",
                linha, __func__
            );
            printf("Token encontrado: %s\n", tokenString[token]);
            exit(-1);
        }

        InserirSimboloOuErro(
            GerarSimbolo(palavraAtual, variavel, &tabela, NULL)
        , false);

        token = Analex();
    }

    if(token != doispontos)
    {
        printf(
            "Erro: esperava-se dois pontos. Linha: %u, função: %s()\n",
            linha, __func__
        );
        exit(1);
    }

    token = Analex();

    if(verificaType(token) == false)
    {
        printf(
            "Erro: esperava-se um tipo. Linha: %u, função: %s()\n",
            linha, __func__
        );
        exit(1);
    }

    for(int i = primeiro; i < tabela.tamanhoLogico; i++)
    {
        tabela.tabela[i].valor = malloc(strlen(palavraAtual) + 1);

        if(tabela.tabela[i].valor == NULL)
            exit(-1);

        strcpy(tabela.tabela[i].valor, palavraAtual);
        tabela.tabela[i].tipo = ObterTipo(palavraAtual);
    }

    token = Analex();

    if(token != pontoevirgula)
    {
        printf(
            "Erro: esperava-se um ponto e virgula. "
            "Linha: %u, função: %s()\n",
            linha, __func__
        );
        exit(1);
    }
}


void AtribuirVariavel()
{
    TipoSemantico tipoAtual = ObterTipo(palavraAtual);
    token = Analex();

    if(token != identificador)
    {
        printf(
            "Erro: Esperava um identificador. Linha: %u, função: %s()\n",
            linha, __func__
        );
        exit(1);
    }

    int primeiro = tabela.tamanhoLogico;

    InserirSimboloOuErro(
        GerarSimbolo(palavraAtual, variavel, &tabela, NULL),
        false
    );

    token = Analex();

    if(token != doispontos)
    {
        printf(
            "Erro: Esperava um dois pontos. Linha: %u, função: %s()\n",
            linha, __func__
        );
        exit(-1);
    }

    token = Analex();

    if(verificaType(token) == false)
    {
        printf(
            "Erro: Esperava um tipo. Linha: %u, função: %s()\n",
            linha, __func__
        );
        exit(-1);
    }
    tabela.tabela[primeiro].valor = malloc(strlen(palavraAtual) + 1);

    if(tabela.tabela[primeiro].valor == NULL)
        exit(-1);

    strcpy(tabela.tabela[primeiro].valor, palavraAtual);
    tabela.tabela[primeiro].tipo = ObterTipo(palavraAtual);
    CompararTipos(tipoAtual, &tabela.tabela[primeiro], linha, "CompararTipos");
    token = Analex();

    if(token != pontoevirgula)
    {
        printf(
            "Erro: Esperava um pontoEVirgula. Linha: %u, função: %s()\n",
            linha, __func__
        );
        exit(-1);
    }
}

void Atribuirrotulo()
{
    token = Analex();

    if(token != numero)
    {
        printf(
            "Erro: esperava a palavra numero! Linha: %u, função: %s()\n",
            linha, __func__
        );
        exit(1);
    }

    Simbolo simbolo = GerarSimbolo(palavraAtual, rotulo, &tabela, NULL);
    InserirSimboloOuErro(simbolo, false);
    tabela.tabela[tabela.tamanhoLogico - 1].identificadorCodigo = GerarRotulo();
    token = Analex();

    if(token != virgula && token != pontoevirgula)
    {
        printf(
            "Erro: esperava a palavra virgula ou pontoEVirgula. "
            "Linha: %u, função: %s()\n",
            linha, __func__
        );
        exit(1);
    }



    while(token == virgula)
    {
        token = Analex();

        if(token != numero)
        {
            printf(
                "Erro: esperava a palavra numero. "
                "Linha: %u, função: %s()\n",
                linha, __func__
            );
            exit(1);
        }

        Simbolo simbolo = GerarSimbolo(
            palavraAtual, rotulo, &tabela, NULL
        );

        InserirSimboloOuErro(simbolo, false);
        tabela.tabela[tabela.tamanhoLogico - 1].identificadorCodigo = GerarRotulo();
        token = Analex();

        if(token != virgula && token != pontoevirgula)
        {
            printf(
                "Erro: esperava a palavra virgula ou pontoEVirgula. "
                "Linha: %u, função: %s()\n",
                linha, __func__
            );
            exit(1);
        }
    }
}

void verificaBloco()
{
    bool corpoEmitido = false;
    while(EhBloco())
    {
        while(token == rotulo)
        {
            Atribuirrotulo();
            token = Analex();
        }

        while(verificaType(token) == true)
        {
            AtribuirVariavel();
            token = Analex();
        }

        while(token == variavel)
        {
            AtribuirTipoImplicito();
            token = Analex();
        }

        while(token == procedimento)
        {
            AtribuirProcedimento();
        }

        while(token == funcao)
        {
            AtribuirFuncao();
        }

        if(token == inicio)
        {
            if(!corpoEmitido)
            {
                if(indiceRotinaAtual >= 0)
                    EmitirCorpoRotina(indiceRotinaAtual);
                else if(!codigoPrincipalIniciado)
                {
                    NADA(rotuloPrincipal);
                    AMEM(ContarPosicoesDoEscopo(0, false));
                    codigoPrincipalIniciado = true;
                }
                corpoEmitido = true;
            }
            VerificaComandoSemRotulo();
        }
    }
}

void verificaProgam()
{
    if(token != programa)
    {
        printf(
            "Erro: esperava a palavra program! Linha: %u, função: %s()\n",
            linha, __func__
        );
        printf("Token encontrado: %s\n", tokenString[token]);
        exit(-1);
    }

    token = Analex();

    if(token != identificador)
    {
        printf(
            "Erro: esperava a palavra identificador! "
            "Linha: %u, função: %s()\n",
            linha, __func__
        );
        exit(1);
    }

    InserirSimboloOuErro(
        GerarSimbolo(palavraAtual, programa, &tabela, NULL),
        false
    );

    token = Analex();

    if(token != abreparenteses)
    {
        printf(
            "Erro: esperava a palavra abreParenteses! "
            "Linha: %u, função: %s()\n",
            linha, __func__
        );
        exit(1);
    }

    while(token != fechaparenteses)
    {
        token = Analex();

        if(token != identificador)
        {
            printf(
                "Erro: esperava a palavra identificador! "
                "Linha: %u, função: %s()\n",
                linha, __func__
            );
            exit(1);
        }

        InserirSimboloOuErro(
            GerarSimbolo(palavraAtual, identificador, &tabela, NULL),
            false
        );

        token = Analex();

        if(token != virgula && token != fechaparenteses)
        {
            printf(
                "Erro: esperava a palavra virgula ou fechaparenteses! "
                "Linha: %u, função: %s()\n",
                linha, __func__
            );
            exit(1);
        }
    }

    token = Analex();

    if(token != pontoevirgula)
    {
        printf(
            "Erro: esperava a palavra virgula ou pontoEVirgula! "
            "Linha: %u, função: %s()\n",
            linha, __func__
        );
        exit(1);
    }

    token = Analex();

    if(!EhBloco())
    {
        printf(
            "Erro: esperava-se um bloco. Linha: %u, função: %s()\n",
            linha, __func__
        );
        printf("Token encontrado: %s\n", tokenString[token]);
        exit(-1);
    }

    verificaBloco();

    if(token != ponto)
    {
        printf(
            "Erro: esperava a palavra ponto! Linha: %u, função: %s()\n",
            linha, __func__
        );
        exit(1);
    }
}

int main()
{
    arquivo = fopen("arq.txt", "r");
    if(arquivo == NULL)
    {
        fprintf(stderr, "Erro: não foi possível abrir arq.txt.\n");
        return EXIT_FAILURE;
    }

    arquivoCodigo = fopen("arq.mepa", "w");
    if(arquivoCodigo == NULL)
    {
        fprintf(stderr, "Erro: não foi possível criar arq.mepa.\n");
        fclose(arquivo);
        return EXIT_FAILURE;
    }

    INPP();
    snprintf(rotuloPrincipal, sizeof(rotuloPrincipal), "MAIN_%lu", GerarRotulo());
    DSVR(rotuloPrincipal);
    token = Analex();

    inicializarTabela(&tabela);
    tabelaCodigo = &tabela;
    verificaProgam();
    PARA();
    fclose(arquivoCodigo);
    arquivoCodigo = NULL;

    ImprimirTabela(&tabela);
    RemoverScopo(&tabela, tabela.ScopoAtual);
    ImprimirTabela(&tabela);
    LiberarTabela(&tabela);

    fclose(arquivo);

    printf("Programa sintaticamente e SEMANTICAMENTE correto!\n");

    return 0;
}
