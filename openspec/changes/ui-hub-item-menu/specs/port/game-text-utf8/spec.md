# Spec Delta

## Purpose

Turns the game's own text, both item names and menu messages, into UTF-8, so port screens drawn with TrueType can show it in every supported game language.

## ADDED Requirements

### Requirement: Game names as UTF-8
Text stored as game strings, such as item names, SHALL decode to UTF-8. ASCII SHALL pass through as is. A `[UNI0xxx]` letter tag SHALL become the character it names. A letter byte, as the game stores a tag once loaded, SHALL become its letter: Latin-1, except that the game's bytes for Œ and œ mean those letters. A decoded name SHALL hold no tag text.

#### Scenario: German name
- **WHEN** a German item name holds the tag for ü
- **THEN** the decoded name holds ü and no tag text

#### Scenario: Loaded French name
- **WHEN** a French item name loaded by the game holds the bytes for é and œ
- **THEN** the decoded name holds é and œ

### Requirement: Game messages as UTF-8
A message from the game's message tables SHALL decode to UTF-8 for the game languages English, French, German, Italian and Spanish. Each glyph code SHALL become its character, and a line break code a newline. The message SHALL end at its end code. Colour, alignment, spacing, wait, voice and page codes SHALL leave no text. A button glyph code SHALL become the name of its button.

#### Scenario: Two-line description
- **WHEN** an item description has a line break code between two lines
- **THEN** the decoded text is the two lines with a newline between them

#### Scenario: Colour codes
- **WHEN** a description highlights a word with colour codes
- **THEN** the decoded text is the plain words, with nothing left from the codes

### Requirement: Message insertions
A message that inserts names or numbers SHALL decode with the names and numbers it is given, in the places their insertion codes mark.

#### Scenario: Number in a description
- **WHEN** a description inserts a number and 30 is given for it
- **THEN** the decoded text shows +30 where the insertion code was, as the game's item menu signs a positive number

### Requirement: Undecodable codes
A code with no known character SHALL be left out of the decoded text, with one warning per code per session.

#### Scenario: Unknown glyph
- **WHEN** a message holds a glyph code that maps to no character
- **THEN** the rest of the message decodes, and one warning names the code
