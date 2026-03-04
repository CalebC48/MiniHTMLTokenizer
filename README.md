
# Mini HTML Tokenizer — Project Specification

## Table of Contents
- [1. Project Overview](#1-project-overview)
  - [1.1 Purpose](#11-purpose)
- [2. Inputs](#2-inputs)
  - [2.1 Supported Input Type](#21-supported-input-type)
  - [2.2 File Type Validation](#22-file-type-validation)
  - [2.3 Encoding Assumptions](#23-encoding-assumptions)
  - [2.4 Input Size Limits](#24-input-size-limits)
- [3. Outputs](#3-outputs)
  - [3.1 Token Stream](#31-token-stream)
  - [3.2 Source Location](#32-source-location)
  - [3.3 Optional Output Modes](#33-optional-output-modes)
- [4. Token Types and Structures](#4-token-types-and-structures)
  - [4.1 TokenType Enumeration](#41-tokentype-enumeration)
  - [4.2 Token Structures](#42-token-structures)
- [5. Supported HTML Subset](#5-supported-html-subset)
  - [5.1 Text Nodes](#51-text-nodes)
  - [5.2 Start Tags](#52-start-tags)
  - [5.3 End Tags](#53-end-tags)
  - [5.4 Self-Closing Tags](#54-self-closing-tags)
  - [5.5 Comments](#55-comments)
  - [5.6 Doctype (Simplified)](#56-doctype-simplified)
- [6. Error Handling](#6-error-handling)
  - [6.1 File/Input Errors](#61-fileinput-errors)
  - [6.2 Tokenization Errors](#62-tokenization-errors)
  - [6.3 Error Reporting Format](#63-error-reporting-format)
- [7. Tokenization Algorithm](#7-tokenization-algorithm)
- [8. Command-Line Interface](#8-command-line-interface)
- [9. Testing Strategy](#9-testing-strategy)
- [10. Implementation Plan](#10-implementation-plan)

---

## 1. Project Overview

### 1.1 Purpose

The goal of this project is to build a small, standalone HTML tokenizer implemented in C++.

The tokenizer reads an input file and produces a stream of tokens representing HTML constructs such as tags, attributes, text, comments, and doctype declarations.

This project is designed as a learning exercise to reinforce:

- String scanning and parsing
- State machine design
- Error handling
- Clean C++ interfaces and data structures
- Testable software design

This tokenizer is not intended to be fully HTML5-compliant.

---

## 2. Inputs

### 2.1 Supported Input Type

The tokenizer supports:

- File input (path to HTML file)

### 2.2 File Type Validation

Accepted extensions:

- .html
- .htm

Non-HTML files are rejected.

### 2.3 Encoding Assumptions

- Input assumed to be UTF-8
- Content processed as std::string bytes
- No advanced encoding validation

### 2.4 Input Size Limits

- Files larger than a configurable size (default: 5MB) produce an error

---

## 3. Outputs

### 3.1 Token Stream

The tokenizer produces a sequence of tokens:

- std::vector<Token> or equivalent stream interface

Each token includes:

- Token type
- Associated data
- Source location information

### 3.2 Source Location

Each token records:

- Line number
- Column number
     - Represents the position of the byte within the current line.
     - Column numbering starts at 1.
     - Easier readability when errors occur.
     - Example:
        
        ```
        Error: unterminated tag
        Line 7, Column 14
        ```

- Byte offset
     - Number of bytes from the start of the input file

### 3.3 Optional Output Modes

- Human-readable output
- JSON output via CLI option

---

## 4. Token Types and Structures

### 4.1 TokenType Enumeration

- Doctype
- StartTag
- EndTag
- Text
- Comment
- EOFToken

Errors may be stored separately rather than represented as tokens.

### 4.2 Token Structures

**DoctypeToken**

- name (string)
- is_valid (boolean)

**TagToken**

- name (lowercased)
- attributes (vector of attributes)
- self_closing (boolean)
- is_start_tag (boolean)

Attribute structure:

- name (string)
- value (string)
- has_value (boolean)

**TextToken**

- data (string)

**CommentToken**

- data (string)

**EOFToken**

- No data

---

## 5. Supported HTML Subset

### 5.1 Text Nodes

- All content outside tags becomes text
- Whitespace preserved
- Adjacent text segments may be merged

### 5.2 Start Tags

Supported examples:

```
<div>
<a href="x">
<img src=test />
```

Rules:

- Tag names consist of letters, digits, and hyphens
- Attributes supported in quoted, unquoted, and boolean forms
- Whitespace allowed between components

### 5.3 End Tags

Supported example:

```
</div>
```

Behavior:

- Parses tag name
- Attributes in end tags produce an error

### 5.4 Self-Closing Tags

Examples:

```
<br/>
<br />
```

Sets self_closing = true.

### 5.5 Comments

Recognizes:

```
<!-- ... -->
```

Unterminated comments generate an error and consume input until end of file.

### 5.6 Doctype (Simplified)

Recognizes:

```
<!DOCTYPE html>
```

(case-insensitive)

Behavior:

- Parses name following DOCTYPE
- Reports error if malformed

---

## 6. Error Handling

The tokenizer must not crash on malformed input. It should report errors and attempt reasonable recovery when possible.

### 6.1 File/Input Errors

- File not found
- Empty file
- File cannot be read
- Non-HTML file input
- File exceeds size limit

### 6.2 Tokenization Errors

Examples:

- Invalid tag start after <
- Unterminated tag
- Invalid or malformed end tag
- Unterminated attribute quotes
- Duplicate attribute names
- Unterminated comment
- Malformed doctype

### 6.3 Error Reporting Format

Each error includes:

- Message
- Line and column

Errors are stored separately from tokens.

---

## 7. Tokenization Algorithm

The tokenizer uses a simple state machine scanning the input sequentially.

Core States:

- ReadingText (default state for processing normal text content)
- ReadingTagOpen (processing a < and determining the type of tag)
- ReadingTagName (consuming characters that form the tag name)
- PreparingToReadAttributeName (determining whether an attribute begins or the tag ends)
- ReadingAttributeName (consuming characters that form an attribute name)
- AfterReadingAttributeName (determining whether an attribute value follows)
- PreparingToReadAttributeValue (determining the format of the attribute value)
- ReadingQuotedAttributeValue (consuming a quoted attribute value)
- ReadingUnquotedAttributeValue (consuming an unquoted attribute value)
- ProcessingSelfClosingTag (handling the / > sequence for self-closing tags)
- ReadingEndTagOpen (processing the </ sequence)
- ReadingMarkupDeclaration (processing declarations such as comments or DOCTYPE)

State transitions depend on current character and context.

---

## 8. Command-Line Interface

**Binary Name**

```
mini_html_tokenizer
```

**Usage**

```bash
mini_html_tokenizer <path>
```

**Options**

- --json — output tokens as JSON
- --errors — print errors to stderr

**Exit Codes**

- 0 — success
- 2 — fatal input error
- 3 — fatal tokenization error

---

## 9. Testing Strategy

Minimum test coverage includes:

1. Plain text input
2. Simple start and end tags
3. Quoted attribute values
4. Unquoted attribute values
5. Boolean attributes
6. Self-closing tags
7. Comment parsing
8. Unterminated comment handling
9. Doctype parsing
10. Random < in text
11. Unterminated tag
12. Unterminated attribute quote
13. Non-HTML file rejection
14. Empty file (expect only EOF token; decide if error or not)
15. File not found
16. File cannot be read
17. File exceeds size limit
18. Malformed end tag
19. Duplicate attribute names
20. Malformed doctype

---

## 10. Implementation Plan

**Phase 1 — Data Structures**

- Define token structures
- Define error structures
- Implement source location tracking

**Phase 2 — Tag and Text Parsing**

- Implement core scanning loop
- Implement tag recognition

**Phase 3 — Attribute Parsing**

- Quoted and unquoted values
- Boolean attributes

**Phase 4 — Comments and Doctype**

- Comment handling
- Simplified doctype handling

**Phase 5 — CLI and File Handling**

- File validation
- Command-line options
- Output formatting

**Phase 6 — Testing and Refinement**

- Add test cases
- Improve error messages
- Validate edge cases
