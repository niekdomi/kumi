# Kumi Grammar Definition (EBNF)

## Lexical Grammar (Tokens)

```ebnf
(* Comments *)
LineComment     = "//" { any-character-except-newline } newline ;
BlockComment    = "/*" { any-character } "*/" ;

(* Whitespace *)
Whitespace      = " " | "\t" | "\r" | "\n" ;

(* Identifiers *)
Identifier      = letter { letter | digit | "-" | "_" } ;
letter          = "a".."z" | "A".."Z" ;
digit           = "0".."9" ;

(* Literals *)
String          = '"' { string-character } '"' ;
string-character = any-character-except-quote | escape-sequence ;
escape-sequence = "\\" ( '"' | "\\" | "n" | "t" | "r" ) ;

Number          = [ "-" ] digit { digit } [ "." digit { digit } ] ;

Boolean         = "true" | "false" ;

(* Keywords *)
Keyword         = "project" | "workspace" | "target" | "dependencies" | "deps"
                | "options" | "global" | "mixin" | "preset" | "features"
                | "testing" | "install" | "package" | "scripts" | "toolchain"
                | "public" | "private" | "interface"
                | "apply" | "inherits" | "extends" | "use-preset" | "export" | "import"
                | "and" | "or" | "not" | "in"
                | "true" | "false" ;

(* Operators and Punctuation *)
At              = "@" ;
Colon           = ":" ;
Semicolon       = ";" ;
Comma           = "," ;
Dot             = "." ;
LeftBrace       = "{" ;
RightBrace      = "}" ;
LeftBracket     = "[" ;
RightBracket    = "]" ;
LeftParen       = "(" ;
RightParen      = ")" ;
Question        = "?" ;
Exclamation     = "!" ;
Equal           = "=" ;
GreaterThan     = ">" ;
LessThan        = "<" ;
GreaterEqual    = ">=" ;
LessEqual       = "<=" ;
EqualEqual      = "==" ;
NotEqual        = "!=" ;
Arrow           = "->" ;
Range           = ".." ;
Ellipsis        = "..." ;
Dollar          = "$" ;
Minus           = "-" ;
```

---

## Syntactic Grammar

### Top Level

```ebnf
(* Root *)
KumiFile        = { Statement } ;

Statement       = ProjectDecl
                | WorkspaceDecl
                | DependenciesDecl
                | TargetDecl
                | OptionsDecl
                | GlobalDecl
                | MixinDecl
                | PresetDecl
                | FeaturesDecl
                | TestingDecl
                | InstallDecl
                | PackageDecl
                | ScriptsDecl
                | ToolchainDecl
                | RootDecl
                | ImportDecl
                | ConditionalBlock
                | ApplyBlock
                | Comment ;

Comment         = LineComment | BlockComment ;
```

---

### Declarations

```ebnf
(* Project *)
ProjectDecl     = "project" Identifier "{" { ProjectProperty } "}" ;
ProjectProperty = PropertyAssignment ;

(* Workspace *)
WorkspaceDecl   = "workspace" "{" { WorkspaceProperty } "}" ;
WorkspaceProperty = PropertyAssignment ;

(* Dependencies *)
DependenciesDecl = ( "dependencies" | "deps" ) "{" { DependencySpec } "}" ;
DependencySpec   = Identifier [ "?" ] ":" DependencyValue [ DependencyOptions ] ";" ;
DependencyValue  = String | Identifier | FunctionCall ;
DependencyOptions = "{" { PropertyAssignment } "}" ;

(* Target *)
TargetDecl      = "target" Identifier [ "extends" Identifier ] "{" { TargetProperty } "}" ;
TargetProperty  = PropertyAssignment
                | VisibilityBlock
                | ConditionalBlock
                | Comment ;

VisibilityBlock = ( "public" | "private" | "interface" ) "{" { PropertyAssignment } "}" ;

(* Options *)
OptionsDecl     = "options" "{" { OptionSpec } "}" ;
OptionSpec      = Identifier ":" OptionType "=" Value [ OptionConstraints ] ";" ;
OptionType      = "bool" | "int" | "string" | "path" | "choice" ;
OptionConstraints = "{" { PropertyAssignment } "}" ;

(* Global *)
GlobalDecl      = "global" "{" { PropertyAssignment } "}" ;

(* Mixin *)
MixinDecl       = "mixin" Identifier "{" { PropertyAssignment | VisibilityBlock } "}" ;

(* Preset *)
PresetDecl      = "preset" Identifier "{" { PropertyAssignment } "}" ;

(* Features *)
FeaturesDecl    = "features" "{" { FeatureSpec } "}" ;
FeatureSpec     = Identifier "{" { PropertyAssignment } "}" ;

(* Testing *)
TestingDecl     = "testing" "{" { PropertyAssignment } "}" ;

(* Install *)
InstallDecl     = "install" "{" { PropertyAssignment | InstallComponent } "}" ;
InstallComponent = "component" Identifier "{" { PropertyAssignment } "}" ;

(* Package *)
PackageDecl     = "package" "{" { PropertyAssignment | PackageGenerator } "}" ;
PackageGenerator = Identifier "{" { PropertyAssignment } "}" ;

(* Scripts *)
ScriptsDecl     = "scripts" "{" { ScriptSpec } "}" ;
ScriptSpec      = Identifier "{" { PropertyAssignment } "}" ;

(* Toolchain *)
ToolchainDecl   = "toolchain" Identifier "{" { PropertyAssignment | ToolchainBlock } "}" ;
ToolchainBlock  = Identifier "{" { PropertyAssignment } "}" ;

(* Root Variables *)
RootDecl        = ":root" "{" { VariableDecl } "}" ;
VariableDecl    = "--" Identifier ":" Value ";" ;

(* Import *)
ImportDecl      = "@import" String ";" ;
```

---

### Control Flow

```ebnf
(* Conditional *)
ConditionalBlock = IfBlock { ElseIfBlock } [ ElseBlock ] ;
IfBlock          = "@if" Condition "{" { Statement } "}" ;
ElseIfBlock      = "@else-if" Condition "{" { Statement } "}" ;
ElseBlock        = "@else" "{" { Statement } "}" ;

Condition        = LogicalExpr ;

LogicalExpr      = ComparisonExpr { ( "and" | "or" ) ComparisonExpr } ;
ComparisonExpr   = UnaryExpr [ ( "==" | "!=" | "<" | ">" | "<=" | ">=" ) UnaryExpr ] ;
UnaryExpr        = [ "not" ] PrimaryExpr ;
PrimaryExpr      = FunctionCall
                 | Identifier
                 | "(" LogicalExpr ")"
                 | Boolean ;

(* Loop *)
ForLoop          = "@for" Identifier "in" Iterable "{" { Statement } "}" ;
Iterable         = List
                 | Range
                 | FunctionCall ;
Range            = Number ".." Number ;

(* Break/Continue *)
BreakStmt        = "@break" ";" ;
ContinueStmt     = "@continue" ";" ;

(* Error/Warning *)
ErrorStmt        = "@error" String ";" ;
WarningStmt      = "@warning" String ";" ;

(* Apply *)
ApplyBlock       = "@apply" FunctionCall "{" { Statement } "}" ;
```

---

### Properties and Values

```ebnf
(* Property Assignment *)
PropertyAssignment = Identifier ":" Value [ ";" ] ;

(* Values *)
Value           = String
                | Number
                | Boolean
                | Identifier
                | List
                | FunctionCall
                | Variable
                | Interpolation ;

List            = "[" [ Value { "," Value } [ "," ] ] "]"
                | Value { "," Value } ;  (* Comma-separated without brackets *)

Variable        = "var(" "--" Identifier ")" ;

Interpolation   = String-with-"${" "--" Identifier "}" ;

(* Function Call *)
FunctionCall    = Identifier "(" [ ArgumentList ] ")" [ FunctionOptions ] ;
ArgumentList    = Argument { "," Argument } ;
Argument        = Value | NamedArgument ;
NamedArgument   = Identifier ":" Value ;
FunctionOptions = "{" { PropertyAssignment } "}" ;
```

---

## Complete EBNF Grammar

```ebnf
(* ============================================ *)
(*        Kumi Build System Grammar (EBNF)     *)
(* ============================================ *)

(* Entry Point *)
KumiFile = { Statement } ;

(* Statements *)
Statement = ProjectDecl | WorkspaceDecl | DependenciesDecl | TargetDecl
          | OptionsDecl | GlobalDecl | MixinDecl | PresetDecl
          | FeaturesDecl | TestingDecl | InstallDecl | PackageDecl
          | ScriptsDecl | ToolchainDecl | RootDecl | ImportDecl
          | ConditionalBlock | ForLoop | ApplyBlock
          | BreakStmt | ContinueStmt | ErrorStmt | WarningStmt ;

(* Top-Level Declarations *)
ProjectDecl     = "project" Identifier "{" { PropertyAssignment } "}" ;
WorkspaceDecl   = "workspace" "{" { PropertyAssignment } "}" ;
DependenciesDecl = ( "dependencies" | "deps" ) "{" { DependencySpec } "}" ;
TargetDecl      = "target" Identifier [ "extends" Identifier ]
                  "{" { TargetContent } "}" ;
OptionsDecl     = "options" "{" { OptionSpec } "}" ;
GlobalDecl      = "global" "{" { PropertyAssignment | ConditionalBlock } "}" ;
MixinDecl       = "mixin" Identifier "{" { PropertyAssignment | VisibilityBlock } "}" ;
PresetDecl      = "preset" Identifier "{" { PropertyAssignment } "}" ;
FeaturesDecl    = "features" "{" { FeatureSpec } "}" ;
TestingDecl     = "testing" "{" { PropertyAssignment } "}" ;
InstallDecl     = "install" "{" { PropertyAssignment | InstallComponent } "}" ;
PackageDecl     = "package" "{" { PropertyAssignment | PackageGenerator } "}" ;
ScriptsDecl     = "scripts" "{" { ScriptSpec } "}" ;
ToolchainDecl   = "toolchain" Identifier "{" { PropertyAssignment | NestedBlock } "}" ;
RootDecl        = ":root" "{" { VariableDecl } "}" ;
ImportDecl      = "@import" String ";" ;

(* Target Content *)
TargetContent   = PropertyAssignment | VisibilityBlock | ConditionalBlock ;
VisibilityBlock = ( "public" | "private" | "interface" )
                  "{" { PropertyAssignment } "}" ;

(* Dependency Specification *)
DependencySpec  = Identifier [ "?" ] ":" DependencyValue
                  [ DependencyOptions ] ";" ;
DependencyValue = String | Identifier | FunctionCall ;
DependencyOptions = "{" { PropertyAssignment } "}" ;

(* Option Specification *)
OptionSpec      = Identifier ":" OptionType "=" Value
                  [ OptionConstraints ] ";" ;
OptionType      = "bool" | "int" | "string" | "path" | "choice" ;
OptionConstraints = "{" { PropertyAssignment } "}" ;

(* Feature Specification *)
FeatureSpec     = Identifier "{" { PropertyAssignment } "}" ;

(* Install Component *)
InstallComponent = "component" Identifier "{" { PropertyAssignment } "}" ;

(* Package Generator *)
PackageGenerator = Identifier "{" { PropertyAssignment } "}" ;

(* Script Specification *)
ScriptSpec      = Identifier "{" { PropertyAssignment } "}" ;

(* Nested Block (for toolchain, etc.) *)
NestedBlock     = Identifier "{" { PropertyAssignment } "}" ;

(* Variable Declaration *)
VariableDecl    = "--" Identifier ":" Value ";" ;

(* Control Flow *)
ConditionalBlock = IfBlock { ElseIfBlock } [ ElseBlock ] ;
IfBlock         = "@if" Condition "{" { Statement } "}" ;
ElseIfBlock     = "@else-if" Condition "{" { Statement } "}" ;
ElseBlock       = "@else" "{" { Statement } "}" ;

ForLoop         = "@for" Identifier "in" Iterable "{" { Statement } "}" ;
Iterable        = List | Range | FunctionCall ;
Range           = Number ".." Number ;

BreakStmt       = "@break" ";" ;
ContinueStmt    = "@continue" ";" ;
ErrorStmt       = "@error" String ";" ;
WarningStmt     = "@warning" String ";" ;

ApplyBlock      = "@apply" FunctionCall "{" { Statement } "}" ;

(* Conditions *)
Condition       = LogicalExpr ;
LogicalExpr     = ComparisonExpr { ( "and" | "or" ) ComparisonExpr } ;
ComparisonExpr  = UnaryExpr [ ComparisonOp UnaryExpr ] ;
ComparisonOp    = "==" | "!=" | "<" | ">" | "<=" | ">=" ;
UnaryExpr       = [ "not" ] PrimaryExpr ;
PrimaryExpr     = FunctionCall | Identifier | Boolean | "(" LogicalExpr ")" ;

(* Property Assignment *)
PropertyAssignment = Identifier ":" Value [ ";" ] ;

(* Values *)
Value           = String | Number | Boolean | Identifier | List
                | FunctionCall | Variable | InterpolatedString ;

List            = "[" [ Value { "," Value } [ "," ] ] "]"
                | Value { "," Value } ;

Variable        = "var" "(" "--" Identifier ")" ;

InterpolatedString = String ;  (* Contains ${--var} syntax *)

(* Function Call *)
FunctionCall    = Identifier "(" [ ArgumentList ] ")" [ FunctionOptions ] ;
ArgumentList    = Argument { "," Argument } ;
Argument        = Value | NamedArgument ;
NamedArgument   = Identifier ":" Value ;
FunctionOptions = "{" { PropertyAssignment } "}" ;

(* Terminals *)
Identifier      = letter { letter | digit | "-" | "_" } ;
String          = '"' { string-char | escape-seq } '"' ;
Number          = [ "-" ] digit { digit } [ "." digit { digit } ] ;
Boolean         = "true" | "false" ;

letter          = "a".."z" | "A".."Z" ;
digit           = "0".."9" ;
string-char     = (* any character except " and \ *) ;
escape-seq      = "\\" ( '"' | "\\" | "n" | "t" | "r" ) ;

(* Comments - ignored by parser *)
LineComment     = "//" { any-char-except-newline } newline ;
BlockComment    = "/*" { any-char } "*/" ;
```

---

## Examples Matching the Grammar

### Example 1: Simple Target

```css
target myapp {
  type: executable;
  sources: "src/**/*.cpp";
  depends: fmt, spdlog;
}
```

**Parse tree:**

```
TargetDecl
├─ Identifier: "myapp"
└─ TargetContent[]
   ├─ PropertyAssignment
   │  ├─ Identifier: "type"
   │  └─ Value: Identifier("executable")
   ├─ PropertyAssignment
   │  ├─ Identifier: "sources"
   │  └─ Value: String("src/**/*.cpp")
   └─ PropertyAssignment
      ├─ Identifier: "depends"
      └─ Value: List[Identifier("fmt"), Identifier("spdlog")]
```

### Example 2: Conditional

```css
@if platform(windows) {
  target myapp {
    defines: PLATFORM_WINDOWS;
  }
} @else  {
  target myapp {
    defines: PLATFORM_UNIX;
  }
}
```

**Parse tree:**

```
ConditionalBlock
├─ IfBlock
│  ├─ Condition: FunctionCall("platform", ["windows"])
│  └─ Statement[]: TargetDecl(...)
└─ ElseBlock
   └─ Statement[]: TargetDecl(...)
```

### Example 3: For Loop

```css
@for module in [core, renderer, physics] {
    target ${module} {
        type: static-lib;
        sources: "modules/${module}/**/*.cpp";
    }
}
```

**Parse tree:**

```
ForLoop
├─ Identifier: "module"
├─ Iterable: List[Identifier("core"), Identifier("renderer"), Identifier("physics")]
└─ Statement[]: TargetDecl(...)
```

---

## Parsing Strategy Notes

### Operator Precedence

```
(highest)
1. Primary (identifiers, literals, function calls)
2. Unary (not)
3. Comparison (==, !=, <, >, <=, >=)
4. Logical AND (and)
5. Logical OR (or)
(lowest)
```

### Semicolon Rules

- Required after property assignments
- Optional on last property in block
- Required after control flow directives (@break, @continue, @error, @warning, @import)
- Not used after blocks ({...})

### List Syntax Ambiguity

```css
/* Both are valid: */
depends: fmt, spdlog; /* Implicit list */
depends: [fmt, spdlog]; /* Explicit list */

/* Parser should accept both */
```

### String Interpolation

```css
/* String with interpolation */
output-name: "myapp-${--version}";

/* Lexer recognizes this as a special string token */
/* Parser extracts variable references during semantic analysis */
```

---

## Grammar Validation

**Valid:**

```css
project myapp { version: "1.0.0"; }
target app { type: executable; sources: "main.cpp"; }
@if config(debug) { global { optimize: none; } }
@for i in 0..5 { target test${i} { } }
```

**Invalid:**

```css
project { version: "1.0.0"; }           /* Missing identifier */
target myapp type: executable;          /* Missing braces */
@if platform(windows) global { };       /* Missing braces for @if */
depends fmt spdlog;                     /* Missing colon */
```

---

## Implementation Notes

### Lexer

- Skip whitespace and comments
- Recognize keywords vs identifiers
- Handle string interpolation (treat ${...} as part of string)
- Line/column tracking for error messages

### Parser (Recursive Descent)

- One parsing function per non-terminal
- Look-ahead of 1 token (LL(1) mostly)
- Error recovery: skip to next semicolon or closing brace
- Build AST with location info for error messages

### AST Nodes

```cpp
struct Node { SourceLocation loc; };
struct TargetDecl : Node { string name; vector<Property> properties; };
struct IfBlock : Node { Condition cond; vector<Statement> body; };
// ... etc
```
