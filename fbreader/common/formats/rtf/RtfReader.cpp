/*
 * FBReader -- electronic book reader
 * Copyright (C) 2004-2006 Nikolay Pultsin <geometer@mawhrin.net>
 * Copyright (C) 2005 Mikhail Sobolev <mss@mawhrin.net>
 * Copyright (C) 2006 Vladimir Sorokin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <iostream>
#include <cctype>

#include <abstract/ZLInputStream.h>

#include "RtfReader.h"

std::map<std::string, RtfReader::RtfCommand*> RtfReader::ourKeywordMap;

static const int rtfStreamBufferSize = 4096;

enum ErrorCode {
  ecOK,                    // Everything's fine!
  ecStackUnderflow,        // Unmatched '}'
  ecUnmatchedBrace,        // RTF ended during an open group.
};

RtfReader::RtfReader(const std::string &encoding) {
  this->encoding = encoding;
  
  if (!encoding.empty()) {
    myConverter = ZLEncodingConverter::createConverter(encoding);
  }
}

RtfReader::~RtfReader() {
}

typedef enum {ipfnCodePage, ipfnSkipDest } IPFN;

RtfReader::RtfCommand::~RtfCommand() {
}

void RtfReader::RtfNewParagraphCommand::run(RtfReader &reader, int*) const {
  reader.newParagraph();
}

RtfReader::RtfFontPropertyCommand::RtfFontPropertyCommand(FontProperty property) : myProperty(property) {
}

void RtfReader::RtfFontPropertyCommand::run(RtfReader &reader, int *parameter) const {
  bool start = (parameter == 0) || (*parameter != 0);
  switch (myProperty) {
    case FONT_BOLD:
      if (reader.myState.Bold != start) {
        reader.myState.Bold = start;
        reader.setFontProperty(FONT_BOLD);
      }
      break;
    case FONT_ITALIC:
      if (reader.myState.Italic != start) {
        reader.myState.Italic = start;
        reader.setFontProperty(FONT_ITALIC);
      }
      break;
    case FONT_UNDERLINED:
      if (reader.myState.Underlined != start) {
        reader.myState.Underlined = start;
        reader.setFontProperty(FONT_UNDERLINED);
      }
      break;
  }
}

RtfReader::RtfAlignmentCommand::RtfAlignmentCommand(AlignmentType alignment) : myAlignment(alignment) {
}

void RtfReader::RtfAlignmentCommand::run(RtfReader &reader, int*) const {
  //std::cerr << "Alignment = " << myAlignment << "\n";
  reader.setAlignment(myAlignment);
}

RtfReader::RtfCharCommand::RtfCharCommand(const std::string &chr) : myChar(chr) {
}

void RtfReader::RtfCharCommand::run(RtfReader &reader, int*) const {
  reader.ecParseCharData(myChar.data(), myChar.length(), false);
}

RtfReader::RtfDestinationCommand::RtfDestinationCommand(Destination destination) : myDestination(destination) {
}

void RtfReader::RtfDestinationCommand::run(RtfReader &reader, int*) const {
  if (reader.myState.rds == myDestination) {
    return;
  }
  reader.myState.rds = myDestination;
  if (myDestination == DESTINATION_PICTURE) {
    reader.myState.ReadDataAsHex = true;
  }
  reader.switchDestination(myDestination, true);
}

void RtfReader::RtfStyleCommand::run(RtfReader &reader, int*) const {
  if (reader.myState.rds == DESTINATION_STYLESHEET) {
    //std::cerr << "Add style index: " << val << "\n";
    
    //sprintf(style_attributes[0], "%i", val);
  } else /*if (myState.rds == rdsContent)*/ {
    //std::cerr << "Set style index: " << val << "\n";

    //sprintf(style_attributes[0], "%i", val);
  }
}

static const char *encoding1251 = "windows-1251";

void RtfReader::RtfCodepageCommand::run(RtfReader &reader, int *parameter) const {
  reader.startElementHandler(0);
  if (parameter != 0) {
    if ((*parameter == 1251) && (reader.encoding != encoding1251)) {
      reader.encoding = encoding1251;
      reader.myConverter = ZLEncodingConverter::createConverter(reader.encoding);
    }
  }
}

void RtfReader::RtfSpecCommand::run(RtfReader &reader, int*) const {
  reader.fSkipDestIfUnk = true;
}

RtfReader::RtfPictureCommand::RtfPictureCommand(const std::string &mimeType) : myMimeType(mimeType) {
}

void RtfReader::RtfPictureCommand::run(RtfReader &reader, int*) const {
  reader.myNextImageMimeType = myMimeType;
}

void RtfReader::RtfFontResetCommand::run(RtfReader &reader, int*) const {
  if (reader.myState.Bold) {
    reader.myState.Bold = false;
    reader.setFontProperty(FONT_BOLD);
  }
  if (reader.myState.Italic) {
    reader.myState.Italic = false;
    reader.setFontProperty(FONT_ITALIC);
  }
  if (reader.myState.Underlined) {
    reader.myState.Underlined = false;
    reader.setFontProperty(FONT_UNDERLINED);
  }
}

void RtfReader::fillKeywordMap() {
  if (ourKeywordMap.empty()) {
    ourKeywordMap["*"] = new RtfSpecCommand();
    ourKeywordMap["ansicpg"] = new RtfCodepageCommand();

    static const char *keywordsToSkip[] = {"buptim", "colortbl", "comment", "creatim", "doccomm", "fonttbl", "footer", "footerf", "footerl", "footerr", "ftncn", "ftnsep", "ftnsepc", "header", "headerf", "headerl", "headerr", "keywords", "operator", "printim", "private1", "revtim", "rxe", "subject", "tc", "txe", "xe", 0};
    RtfCommand *skipCommand = new RtfDestinationCommand(DESTINATION_NONE);
    for (const char **i = keywordsToSkip; *i != 0; i++) {
      ourKeywordMap[*i] = skipCommand;
    }
    ourKeywordMap["info"] = new RtfDestinationCommand(DESTINATION_INFO);
    ourKeywordMap["title"] = new RtfDestinationCommand(DESTINATION_TITLE);
    ourKeywordMap["author"] = new RtfDestinationCommand(DESTINATION_AUTHOR);
    ourKeywordMap["pict"] = new RtfDestinationCommand(DESTINATION_PICTURE);
    ourKeywordMap["stylesheet"] = new RtfDestinationCommand(DESTINATION_STYLESHEET);
    ourKeywordMap["footnote"] = new RtfDestinationCommand(DESTINATION_FOOTNOTE);

    RtfCommand *newParagraphCommand = new RtfNewParagraphCommand();
    ourKeywordMap["\n"] = newParagraphCommand;
    ourKeywordMap["\r"] = newParagraphCommand;
    ourKeywordMap["par"] = newParagraphCommand;

    ourKeywordMap["\x09"] = new RtfCharCommand("\x09");
    ourKeywordMap["\\"] = new RtfCharCommand("\\");
    ourKeywordMap["{"] = new RtfCharCommand("{");
    ourKeywordMap["}"] = new RtfCharCommand("}");
    ourKeywordMap["bullet"] = new RtfCharCommand("\xE2\x80\xA2");     // &bullet;
    ourKeywordMap["endash"] = new RtfCharCommand("\xE2\x80\x93");     // &ndash;
    ourKeywordMap["emdash"] = new RtfCharCommand("\xE2\x80\x94");     // &mdash;
    ourKeywordMap["~"] = new RtfCharCommand("\xC0\xA0");              // &nbsp;
    ourKeywordMap["enspace"] = new RtfCharCommand("\xE2\x80\x82");    // &emsp;
    ourKeywordMap["emspace"] = new RtfCharCommand("\xE2\x80\x83");    // &ensp;
    ourKeywordMap["lquote"] = new RtfCharCommand("\xE2\x80\x98");     // &lsquo;
    ourKeywordMap["rquote"] = new RtfCharCommand("\xE2\x80\x99");     // &rsquo;
    ourKeywordMap["ldblquote"] = new RtfCharCommand("\xE2\x80\x9C");  // &ldquo;
    ourKeywordMap["rdblquote"] = new RtfCharCommand("\xE2\x80\x9D");  // &rdquo;

    ourKeywordMap["jpegblip"] = new RtfPictureCommand("image/jpeg");
    ourKeywordMap["pngblip"] = new RtfPictureCommand("image/png");

    ourKeywordMap["s"] = new RtfStyleCommand();

    ourKeywordMap["qc"] = new RtfAlignmentCommand(ALIGN_CENTER);
    ourKeywordMap["ql"] = new RtfAlignmentCommand(ALIGN_LEFT);
    ourKeywordMap["qr"] = new RtfAlignmentCommand(ALIGN_RIGHT);
    ourKeywordMap["qj"] = new RtfAlignmentCommand(ALIGN_JUSTIFY);
    ourKeywordMap["pard"] = new RtfAlignmentCommand(ALIGN_UNDEFINED);

    ourKeywordMap["b"] = new RtfFontPropertyCommand(FONT_BOLD);
    ourKeywordMap["i"] = new RtfFontPropertyCommand(FONT_ITALIC);
    ourKeywordMap["u"] = new RtfFontPropertyCommand(FONT_UNDERLINED);
    ourKeywordMap["plain"] = new RtfFontResetCommand();
  }
}

int RtfReader::parseDocument() {
  enum {
    READ_NORMAL_DATA,
    READ_BINARY_DATA,
    READ_HEX_SYMBOL,
    READ_KEYWORD,
    READ_KEYWORD_PARAMETER
  } parserState = READ_NORMAL_DATA;

  std::string keyword;
  std::string parameterString;
  std::string hexString;
  int imageStartOffset = -1;

  while (!myIsInterrupted) {
    const char *ptr = myStreamBuffer;
    const char *end = myStreamBuffer + myStream->read(myStreamBuffer, rtfStreamBufferSize);
    if (ptr == end) {
      break;
    }
    const char *dataStart = ptr;
    bool readNextChar = true;
    while (ptr != end) {
      switch (parserState) {
        case READ_BINARY_DATA:
          // TODO: optimize
          ecParseCharData(ptr, 1);
          myBinaryDataSize--;
          if (myBinaryDataSize == 0) {
            parserState = READ_NORMAL_DATA;
          }
          break;
        case READ_NORMAL_DATA:
          switch (*ptr) {
            case '{':
              if (ptr > dataStart) {
                ecParseCharData(dataStart, ptr - dataStart);
              }
              dataStart = ptr + 1;
              myStateStack.push(myState);
              myState.ReadDataAsHex = false;
              break;
            case '}':
            {
              if (ptr > dataStart) {
                ecParseCharData(dataStart, ptr - dataStart);
              }
              dataStart = ptr + 1;

              if (imageStartOffset >= 0) {
                int imageSize = myStream->offset() + (ptr - end) - imageStartOffset;
                insertImage(myNextImageMimeType, myFileName, imageStartOffset, imageSize);
                imageStartOffset = -1;
              }

              if (myStateStack.empty()) {
                return ecStackUnderflow;
              }
              
              if (myState.rds != myStateStack.top().rds) {
                switchDestination(myState.rds, false);
              }
              
              bool oldItalic = myState.Italic;
              bool oldBold = myState.Bold;
              bool oldUnderlined = myState.Underlined;
              myState = myStateStack.top();
              myStateStack.pop();
          
              if (myState.Italic != oldItalic) {
                setFontProperty(FONT_ITALIC);
              }
              if (myState.Bold != oldBold) {
                setFontProperty(FONT_BOLD);
              }
              if (myState.Underlined != oldUnderlined) {
                setFontProperty(FONT_UNDERLINED);
              }
							// TODO: reset alignment
              
              break;
            }
            case '\\':
              if (ptr > dataStart) {
                ecParseCharData(dataStart, ptr - dataStart);
              }
              dataStart = ptr + 1;
              keyword.erase();
              parserState = READ_KEYWORD;
              break;
            case 0x0d:
            case 0x0a:      // cr and lf are noise characters...
              if (ptr > dataStart) {
                ecParseCharData(dataStart, ptr - dataStart);
              }
              dataStart = ptr + 1;
              break;
            default:
              if (myState.ReadDataAsHex) {
                if (imageStartOffset == -1) {
                  imageStartOffset = myStream->offset() + (ptr - end);
                }
              }
              break;
          }
          break;
        case READ_HEX_SYMBOL:
          hexString += *ptr;
          if (hexString.size() == 2) {
            char ch = strtol(hexString.c_str(), 0, 16); 
            hexString.erase();
            ecParseCharData(&ch, 1);
            parserState = READ_NORMAL_DATA;
            dataStart = ptr + 1;
          }
          break;
        case READ_KEYWORD:
          if (!isalpha(*ptr)) {
            if ((ptr == dataStart) && (keyword.empty())) {
              if (*ptr == '\'') {
                parserState = READ_HEX_SYMBOL;
              } else {
                keyword = *ptr;
                ecTranslateKeyword(keyword, 0, false);
                parserState = READ_NORMAL_DATA;
              }
              dataStart = ptr + 1;
            } else {
              keyword.append(dataStart, ptr - dataStart);
              if ((*ptr == '-') || isdigit(*ptr)) {
                dataStart = ptr;
                parserState = READ_KEYWORD_PARAMETER;
              } else {
                readNextChar = *ptr == ' ';
                ecTranslateKeyword(keyword, 0, false);
                parserState = READ_NORMAL_DATA;
                dataStart = readNextChar ? ptr + 1 : ptr;
              }
            }
          }
          break;
        case READ_KEYWORD_PARAMETER:
          if (!isdigit(*ptr)) {
            parameterString.append(dataStart, ptr - dataStart);
            int param = atoi(parameterString.c_str());
            parameterString.erase();
            readNextChar = *ptr == ' ';
            if ((keyword == "bin") && (param > 0)) {
              myBinaryDataSize = param;
              parserState = READ_BINARY_DATA;
            } else {
              ecTranslateKeyword(keyword, param, true);
              parserState = READ_NORMAL_DATA;
            }
            dataStart = readNextChar ? ptr + 1 : ptr;
          }
          break;
      }
      if (readNextChar) {
        ptr++;
      } else {
        readNextChar = true;
      }
    }
    if (dataStart < end) {
      switch (parserState) {
        case READ_NORMAL_DATA:
          ecParseCharData(dataStart, end - dataStart);
        case READ_KEYWORD:
          keyword.append(dataStart, end - dataStart);
          break;
        case READ_KEYWORD_PARAMETER:
          parameterString.append(dataStart, end - dataStart);
          break;
        default:
          break;
      }
    }
  }
  
  return (myIsInterrupted || myStateStack.empty()) ? ecOK : ecUnmatchedBrace;
}

void RtfReader::ecTranslateKeyword(const std::string &keyword, int param, bool fParam) {
  if (myState.rds == DESTINATION_SKIP) {
    fSkipDestIfUnk = false;
    return;
  }

  std::map<std::string, RtfCommand*>::const_iterator it = ourKeywordMap.find(keyword);
  
  if (it == ourKeywordMap.end()) {
    if (fSkipDestIfUnk)     // if this is a new destination
      myState.rds = DESTINATION_SKIP;      // skip the destination
                  // else just discard it
    fSkipDestIfUnk = false;

//    DPRINT("unknown keyword\n");    
    return;
  }

  fSkipDestIfUnk = false;

  it->second->run(*this, fParam ? &param : 0);
}


void RtfReader::ecParseCharData(const char *data, size_t len, bool convert) {
  if (myState.rds != DESTINATION_SKIP) {
    addCharData(data, len, convert);
  }
}

void RtfReader::interrupt() {
  myIsInterrupted = true;
}

bool RtfReader::readDocument(const std::string &fileName) {
  myFileName = fileName;
  myStream = ZLFile(fileName).inputStream();
  if (myStream.isNull() || !myStream->open()) {
      return false;
  }

  fillKeywordMap();

  myStreamBuffer = new char[rtfStreamBufferSize];
  
  myIsInterrupted = false;
  startDocumentHandler();

  fSkipDestIfUnk = false;

  myState.alignment = ALIGN_UNDEFINED;
  myState.Italic = false;
  myState.Bold = false;
  myState.Underlined = false;
  myState.rds = DESTINATION_NONE;
  myState.ReadDataAsHex = false;

  int ret = parseDocument();
  bool code = ret == ecOK;
  if (!code) {
    std::cerr << "parse failed: " << ret << "\n";
  }
  endDocumentHandler();

  while (!myStateStack.empty()) {
    myStateStack.pop();
  }
  
  delete[] myStreamBuffer;
  myStream->close();
  
  return code;
}