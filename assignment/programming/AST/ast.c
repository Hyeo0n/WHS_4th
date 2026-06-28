#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string.h>
#include "json_c.c"  
 
/* 파일 전체를 읽어서 문자열로 반환 */
char *read_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        printf("[Error] 파일 열기 실패: %s\n", path);
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
 
    char *buf = (char *)malloc(size + 1);
    if (buf == NULL) {
        printf("[Error] 메모리 할당 실패\n");
        fclose(fp);
        return NULL;
    }
    fread(buf, 1, size, fp);
    buf[size] = '\0';
    fclose(fp);
    return buf;
}
 
/* 리턴 타입 추출 */
void print_return_type(json_value func_node) {
    // FuncDecl -> type 경로 지정
    json_value type_decl = json_get(func_node, "decl", "type", "type");
    char *nodetype = json_get_string(type_decl, "_nodetype");
 
    if (nodetype && strcmp(nodetype, "PtrDecl") == 0) {
        // 포인터 리턴타입: char*..
        char *rtype = json_get_string(type_decl, "type", "type", "names", 0);
        printf("%s*", rtype ? rtype : "?");
    } else {
        // 일반 리턴타입: int, void..
        char *rtype = json_get_string(type_decl, "type", "names", 0);
        printf("%s", rtype ? rtype : "?");
    }
}
 
/* 파라미터 추출 */
void print_params(json_value func_node) {
    // decl->type->args 경로 지정
    json_value args = json_get(func_node, "decl", "type", "args");
 
    // args가 null인 경우는 파라미터 없는 경우
    if (json_is_null(args) || args.type == JSON_UNDEFINED) {
        printf("(void)");
        return;
    }
    // params 경로 지정
    json_value params = json_get(args, "params");
    int param_len = json_len(params);
 
    if (param_len == 0) {
        printf("(void)");
        return;
    }
 
    printf("(");
    for (int i = 0; i < param_len; i++) {
        json_value param = json_get(params, i);
        char *pname = json_get_string(param, "name");
        json_value ptype = json_get(param, "type");
        char *ptype_node = json_get_string(ptype, "_nodetype");
 
        if (ptype_node && strcmp(ptype_node, "PtrDecl") == 0) {    // 포인터 타입일 경우
            char *tname = json_get_string(ptype, "type", "type", "names", 0);
            printf("%s* %s", tname ? tname : "?", pname ? pname : "?");
        } else {    // 일반 타입일 경우
            char *tname = json_get_string(ptype, "type", "names", 0);
            printf("%s %s", tname ? tname : "?", pname ? pname : "?");
        }
 
        if (i < param_len - 1) printf(", ");
    }
    printf(")");
}
 
/* if 개수 재귀 탐색 */
int count_if_nodes(json_value node) {
    int count = 0;
 
    if (node.type == JSON_OBJECT) {
        // if 노드인지 확인
        char *nodetype = json_get_string(node, "_nodetype");
        if (nodetype && strcmp(nodetype, "If") == 0) {
            count++;
        }
        // 오브젝트의 모든 값: 재귀 탐색
        json_object *obj = (json_object *)node.value;
        for (int i = 0; i <= obj->last_index; i++) {
            count += count_if_nodes(obj->values[i]);
        }
    } else if (node.type == JSON_ARRAY) {
        // 배열의 모든 요소: 재귀 탐색
        json_array *arr = (json_array *)node.value;
        for (int i = 0; i <= arr->last_index; i++) {
            count += count_if_nodes(arr->values[i]);
        }
    }
    return count;
}
 
/* 소스코드 복원 - 들여쓰기 출력 */
void print_indent(int indent) {
    // indent 깊이 만큼 공백 4칸 출력
    for (int i = 0; i < indent; i++) printf("    ");
}

/* 소스코드 복원 - 표현식(Expression) 출력 */
void print_expr(json_value node) {
    if (node.type == JSON_NULL || node.type == JSON_UNDEFINED) return;

    char *nt = json_get_string(node, "_nodetype");
    if (!nt) return;

    if (strcmp(nt, "ID") == 0) {
        // 변수명, 함수명 등 식별자 출력
        char *name = json_get_string(node, "name");
        printf("%s", name ? name : "?");
    }
    else if (strcmp(nt, "Constant") == 0) {
        // 숫자, 문자 등 상수값 출력
        char *val = json_get_string(node, "value");
        printf("%s", val ? val : "0");
    }
    else if (strcmp(nt, "BinaryOp") == 0) {
        // 이항 연산자 출력 (ex. a == b, a+i)
        char *op = json_get_string(node, "op");
        printf("(");
        print_expr(json_get(node, "left"));  // 왼쪽 피연산자
        printf(" %s ", op ? op : "?");       // 연산자
        print_expr(json_get(node, "right")); // 오른쪽 피연산자
        printf(")");
    }
    else if (strcmp(nt, "UnaryOp") == 0) {
        // 단항 연산자 출력 (ex. i++, *s)
        char *op = json_get_string(node, "op");
        json_value expr = json_get(node, "expr");
        if (op && (strcmp(op, "p++") == 0 || strcmp(op, "p--") == 0)) {
            // 후위 연산자 (i++, i--)
            print_expr(expr);
            printf("%s", op + 1);
        } else {
            // 전위 연산자 (ex.*p)
            printf("%s", op ? op : "?");
            print_expr(expr);
        }
    }
    else if (strcmp(nt, "FuncCall") == 0) {
        // 함수 호출 출력
        print_expr(json_get(node, "name"));  // 함수명
        printf("(");
        json_value args = json_get(node, "args");
        if (!json_is_null(args) && args.type != JSON_UNDEFINED) {
            json_value exprs = json_get(args, "exprs");
            int len = json_len(exprs);
            for (int i = 0; i < len; i++) {
                if (i > 0) printf(", ");  // 인자 구분
                print_expr(json_get(exprs, i));
            }
        }
        printf(")");
    }
    else if (strcmp(nt, "ArrayRef") == 0) {
        // 배열 접근 출력
        print_expr(json_get(node, "name"));  // 배열명
        printf("[");
        print_expr(json_get(node, "subscript"));  // 인덱스
        printf("]");
    }
    else if (strcmp(nt, "Assignment") == 0) {
        // 대입 연산 출력 (ex. a = a+1)
        char *op = json_get_string(node, "op");
        print_expr(json_get(node, "lvalue"));  // 왼쪽의 대입 대상
        printf(" %s ", op ? op : "=");  // 대입 연산자
        print_expr(json_get(node, "rvalue"));  // 오른쪽의 대입 값
    }
    else if (strcmp(nt, "Cast") == 0) {
        // 형변환 출력
        json_value to_type = json_get(node, "to_type");
        char *tname = json_get_string(to_type, "type", "names", 0);
        printf("(%s)", tname ? tname : "?");
        print_expr(json_get(node, "expr"));
    }
    else {
        // 모든 경우에 해당하지 않으면 주석 처리
        printf("/* %s */", nt);
    }
}

/* 소스코드 복원 - 문장(Statement) 출력 */
void print_stmt(json_value node, int indent);

void print_compound(json_value body, int indent) {
    // block_items 배열 순회 -> 각 문장 출력
    json_value items = json_get(body, "block_items");
    if (json_is_null(items) || items.type == JSON_UNDEFINED) return;
    int len = json_len(items);
    for (int i = 0; i < len; i++) {
        print_stmt(json_get(items, i), indent);
    }
}

void print_stmt(json_value node, int indent) {
    if (node.type == JSON_NULL || node.type == JSON_UNDEFINED) return;

    char *nt = json_get_string(node, "_nodetype");
    if (!nt) return;

    if (strcmp(nt, "Return") == 0) {
        // return문 출력
        print_indent(indent);
        printf("return");
        json_value expr = json_get(node, "expr");
        if (!json_is_null(expr) && expr.type != JSON_UNDEFINED) {
            printf(" ");
            print_expr(expr);  // 반환값 출력
        }
        printf(";\n");
    }
    else if (strcmp(nt, "Assignment") == 0) {
        // 대입문 출력
        print_indent(indent);
        print_expr(node);
        printf(";\n");
    }
    else if (strcmp(nt, "FuncCall") == 0) {
        // 함수 호출 출력
        print_indent(indent);
        print_expr(node);
        printf(";\n");
    }
    else if (strcmp(nt, "Decl") == 0) {
        // 변수 선언문 출력
        print_indent(indent);
        char *name = json_get_string(node, "name");
        json_value type_node = json_get(node, "type");
        char *type_nt = json_get_string(type_node, "_nodetype");

        if (type_nt && strcmp(type_nt, "PtrDecl") == 0) {
            // 포인터 변수 선언
            char *tname = json_get_string(type_node, "type", "type", "names", 0);
            printf("%s* %s", tname ? tname : "?", name ? name : "?");
        } else {
            // 일반 변수 선언
            char *tname = json_get_string(type_node, "type", "names", 0);
            printf("%s %s", tname ? tname : "?", name ? name : "?");
        }

        json_value init = json_get(node, "init");
        if (!json_is_null(init) && init.type != JSON_UNDEFINED) {
            printf(" = ");
            print_expr(init);  // 초기값 출력
        }
        printf(";\n");
    }
    else if (strcmp(nt, "If") == 0) {
        // if문 출력
        print_indent(indent);
        printf("if (");
        print_expr(json_get(node, "cond"));  // 조건
        printf(") {\n");

        // 조건이 참일 때 코드
        json_value iftrue = json_get(node, "iftrue");
        char *iftrue_nt = json_get_string(iftrue, "_nodetype");
        if (iftrue_nt && strcmp(iftrue_nt, "Compound") == 0)
            print_compound(iftrue, indent + 1);
        else
            print_stmt(iftrue, indent + 1);
        print_indent(indent);
        printf("}");

        // 조건이 거짓일 때 코드
        json_value iffalse = json_get(node, "iffalse");
        if (!json_is_null(iffalse) && iffalse.type != JSON_UNDEFINED) {
            printf(" else {\n");
            char *iffalse_nt = json_get_string(iffalse, "_nodetype");
            if (iffalse_nt && strcmp(iffalse_nt, "Compound") == 0)
                print_compound(iffalse, indent + 1);
            else
                print_stmt(iffalse, indent + 1);
            print_indent(indent);
            printf("}");
        }
        printf("\n");
    }
    else if (strcmp(nt, "While") == 0) {
        // while 문 출
        print_indent(indent);
        printf("while (");
        print_expr(json_get(node, "cond"));  // 반복 조건식
        printf(") {\n");

        json_value stmt = json_get(node, "stmt");
        char *stmt_nt = json_get_string(stmt, "_nodetype");
        if (stmt_nt && strcmp(stmt_nt, "Compound") == 0)
            print_compound(stmt, indent + 1);  // 블록문
        else
            print_stmt(stmt, indent + 1);  // 단일문
        print_indent(indent);
        printf("}\n");
    }
    else if (strcmp(nt, "Compound") == 0) {
        // 중괄호 블록 { } 처리
        print_compound(node, indent);
    }
    else {
        // 처리하지 못한 노드 TODO 주석 표시
        print_indent(indent);
        printf("/* TODO: %s */\n", nt);
    }
}

/* 소스코드 복원 - 함수 전체 출력 */
void print_func_restore(json_value func_node) {
    print_return_type(func_node);  // 리턴타입 출력
    printf(" ");
    char *fname = json_get_string(func_node, "decl", "name");  // 함수 이름 출력
    printf("%s", fname ? fname : "?");   // 파라미터 출력
    print_params(func_node);
    printf(" {\n");
    print_compound(json_get(func_node, "body"), 1);  // 함수 본문 출력
    printf("}\n\n");
}

/* main 함수 */
int main(void) {
    SetConsoleOutputCP(CP_UTF8);  // 한글 깨짐 방지
    printf("==============================================\n");
    printf("         AST 분석 프로그램 (ast.json)         \n");
    printf("==============================================\n\n");
 
    // 1) 파일 읽기
    char *buf = read_file("ast.json");
    if (buf == NULL) return 1;
 
    // 2) JSON 파싱
    json_value json = json_create(buf);
    json_value ext  = json_get(json, "ext");
    int ext_len     = json_len(ext);
 
    // 3) FuncDef 노드 탐색 + 분석
    int func_count = 0;
 
    // 함수 개수 카운트
    for (int i = 0; i < ext_len; i++) {
        json_value node = json_get(ext, i);
        char *nodetype  = json_get_string(node, "_nodetype");
        if (nodetype && strcmp(nodetype, "FuncDef") == 0) {
            func_count++;
        }
    }
 
    printf("[1] 총 함수 개수: %d\n\n", func_count);
 
    // 4) 각 함수 상세 정보 출력
    printf("[2] 함수 상세 정보\n");
    printf("----------------------------------------------\n");
 
    int idx = 0;
    for (int i = 0; i < ext_len; i++) {
        json_value node = json_get(ext, i);
        char *nodetype  = json_get_string(node, "_nodetype");
 
        if (nodetype && strcmp(nodetype, "FuncDef") == 0) {
            idx++;
 
            // 함수 이름
            char *fname = json_get_string(node, "decl", "name");
 
            // 위치 (파일명:줄번호)
            char *coord = json_get_string(node, "coord");
 
            // if 개수
            json_value body = json_get(node, "body");
            int if_count = count_if_nodes(body);
 
            printf("함수 #%02d\n", idx);
            printf("  이름      : %s\n", fname ? fname : "?");
            printf("  위치      : %s\n", coord ? coord : "?");
            printf("  리턴타입  : ");
            print_return_type(node);
            printf("\n");
            printf("  파라미터  : ");
            print_params(node);
            printf("\n");
            printf("  if 개수   : %d\n", if_count);
            printf("----------------------------------------------\n");
        }
    }
 
    // 5) 소스코드 복원 (심화) - 36개 전체
    printf("\n[3] 소스코드 복원 (심화)\n");
    printf("==============================================\n\n");

    for (int i = 0; i < ext_len; i++) {
        json_value node = json_get(ext, i);
        char *nodetype  = json_get_string(node, "_nodetype");
        if (!nodetype || strcmp(nodetype, "FuncDef") != 0) continue;  // FuncDef 아니면 건너뜀
        print_func_restore(node);  // 함수 하나씩 복원 출력
    }

    printf("==============================================\n");
    printf("              분석 완료\n");
    printf("==============================================\n");

    free(buf);
    return 0;
}
