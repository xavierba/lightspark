/**************************************************************************
    Lightspark, a free flash player implementation

    Copyright (C) 2009-2013  Alessandro Pignotti (a.pignotti@sssup.it)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

#include "scripting/abc.h"
#include "scripting/flash/text/flashtext.h"
#include "scripting/class.h"
#include "compat.h"
#include "backends/geometry.h"
#include "backends/graphics.h"
#include "scripting/argconv.h"
#include <3rdparty/pugixml/src/pugixml.hpp>

using namespace std;
using namespace lightspark;

void lightspark::AntiAliasType::sinit(Class_base* c)
{
	CLASS_SETUP_NO_CONSTRUCTOR(c, ASObject, CLASS_FINAL | CLASS_SEALED);
	c->setVariableAtomByQName("ADVANCED",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"advanced"),CONSTANT_TRAIT);
	c->setVariableAtomByQName("NORMAL",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"normal"),CONSTANT_TRAIT);
}

void ASFont::sinit(Class_base* c)
{
	CLASS_SETUP_NO_CONSTRUCTOR(c, ASObject, CLASS_SEALED);
	c->setDeclaredMethodByQName("enumerateFonts","",Class<IFunction>::getFunction(c->getSystemState(),enumerateFonts),NORMAL_METHOD,false);
	c->setDeclaredMethodByQName("registerFont","",Class<IFunction>::getFunction(c->getSystemState(),registerFont),NORMAL_METHOD,false);
	c->setDeclaredMethodByQName("hasGlyphs","",Class<IFunction>::getFunction(c->getSystemState(),hasGlyphs),NORMAL_METHOD,true);

	REGISTER_GETTER(c,fontName);
	REGISTER_GETTER(c,fontStyle);
	REGISTER_GETTER(c,fontType);
}
void ASFont::SetFont(tiny_string& fontname,bool is_bold,bool is_italic, bool is_Embedded, bool is_EmbeddedCFF)
{
	fontName = fontname;
	fontStyle = (is_bold ? 
					 (is_italic ? "boldItalic" : "bold") :
					 (is_italic ? "italic" : "regular")
					 );
	fontType = (is_Embedded ?
					(is_EmbeddedCFF ? "embeddedCFF" : "embedded") :
					"device");
}

std::vector<asAtom> *ASFont::getFontList()
{
	static std::vector<asAtom> fontlist;
	return &fontlist;
}
ASFUNCTIONBODY_GETTER(ASFont, fontName);
ASFUNCTIONBODY_GETTER(ASFont, fontStyle);
ASFUNCTIONBODY_GETTER(ASFont, fontType);

ASFUNCTIONBODY_ATOM(ASFont,enumerateFonts)
{
	bool enumerateDeviceFonts=false;
	ARG_UNPACK_ATOM(enumerateDeviceFonts,false);

	if (enumerateDeviceFonts)
		LOG(LOG_NOT_IMPLEMENTED,"Font::enumerateFonts: flag enumerateDeviceFonts is not handled");
	Array* res = Class<Array>::getInstanceSNoArgs(sys);
	std::vector<asAtom>* fontlist = getFontList();
	for(auto i = fontlist->begin(); i != fontlist->end(); ++i)
	{
		ASATOM_INCREF((*i));
		res->push(*i);
	}
	ret = asAtom::fromObject(res);
}
ASFUNCTIONBODY_ATOM(ASFont,registerFont)
{
	_NR<ASObject> fontclass;
	ARG_UNPACK_ATOM(fontclass);
	if (!fontclass->is<Class_inherit>())
		throwError<ArgumentError>(kCheckTypeFailedError,
								  fontclass->getClassName(),
								  Class<ASFont>::getClass(sys)->getQualifiedClassName());
	ASFont* font = new (fontclass->as<Class_base>()->memoryAccount) ASFont(fontclass->as<Class_base>());
	fontclass->as<Class_base>()->setupDeclaredTraits(font);
	font->constructionComplete();
	font->setConstructIndicator();
	getFontList()->push_back(asAtom::fromObject(font));
}
ASFUNCTIONBODY_ATOM(ASFont,hasGlyphs)
{
	ASFont* th = obj.as<ASFont>();
	tiny_string text;
	ARG_UNPACK_ATOM(text);
	if (th->fontType == "embedded")
	{
		DefineFont3Tag* f = sys->mainClip->getEmbeddedFont(th->fontName);
		if (f)
		{
			ret.setBool(f->hasGlyphs(text));
			return;
		}
	}
	LOG(LOG_NOT_IMPLEMENTED,"Font.hasGlyphs always returns true for not embedded fonts:"<<text<<" "<<th->fontName<<" "<<th->fontStyle<<" "<<th->fontType);
	ret.setBool(true);
}
TextField::TextField(Class_base* c, const TextData& textData, bool _selectable, bool readOnly)
	: InteractiveObject(c), TextData(textData), TokenContainer(this), type(ET_READ_ONLY),
	  antiAliasType(AA_NORMAL), gridFitType(GF_PIXEL),
	  textInteractionMode(TI_NORMAL), alwaysShowSelection(false),
	  caretIndex(0), condenseWhite(false), displayAsPassword(false),
	  embedFonts(false), maxChars(0), mouseWheelEnabled(true),
	  selectable(_selectable), selectionBeginIndex(0), selectionEndIndex(0),
	  sharpness(0), thickness(0), useRichTextClipboard(false)
{
	subtype=SUBTYPE_TEXTFIELD;
	if (!readOnly)
	{
		type = ET_EDITABLE;
		tabEnabled = true;
	}
}

void TextField::sinit(Class_base* c)
{
	CLASS_SETUP_NO_CONSTRUCTOR(c, InteractiveObject, CLASS_SEALED);

	// methods
	c->setDeclaredMethodByQName("appendText","",Class<IFunction>::getFunction(c->getSystemState(),TextField:: appendText),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("getTextFormat","",Class<IFunction>::getFunction(c->getSystemState(),_getTextFormat),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("setTextFormat","",Class<IFunction>::getFunction(c->getSystemState(),_setTextFormat),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("getLineIndexAtPoint","",Class<IFunction>::getFunction(c->getSystemState(),_getLineIndexAtPoint),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("getLineIndexOfChar","",Class<IFunction>::getFunction(c->getSystemState(),_getLineIndexOfChar),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("getLineLength","",Class<IFunction>::getFunction(c->getSystemState(),_getLineLength),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("getLineMetrics","",Class<IFunction>::getFunction(c->getSystemState(),_getLineMetrics),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("getLineOffset","",Class<IFunction>::getFunction(c->getSystemState(),_getLineOffset),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("getLineText","",Class<IFunction>::getFunction(c->getSystemState(),_getLineText),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("replaceSelectedText","",Class<IFunction>::getFunction(c->getSystemState(),_replaceSelectedText),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("replaceText","",Class<IFunction>::getFunction(c->getSystemState(),_replaceText),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("setSelection","",Class<IFunction>::getFunction(c->getSystemState(),_setSelection),NORMAL_METHOD,true);

	// properties
	c->setDeclaredMethodByQName("antiAliasType","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_getAntiAliasType),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("antiAliasType","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_setAntiAliasType),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("autoSize","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_setAutoSize),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("autoSize","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_getAutoSize),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("defaultTextFormat","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_getDefaultTextFormat),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("defaultTextFormat","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_setDefaultTextFormat),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("gridFitType","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_getGridFitType),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("gridFitType","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_setGridFitType),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("height","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_getHeight),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("height","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_setHeight),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("htmlText","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_getHtmlText),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("htmlText","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_setHtmlText),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("length","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_getLength),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("text","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_getText),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("text","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_setText),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("textHeight","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_getTextHeight),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("textWidth","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_getTextWidth),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("width","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_getWidth),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("width","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_setWidth),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("wordWrap","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_setWordWrap),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("wordWrap","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_getWordWrap),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("numLines","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_getNumLines),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("maxScrollH","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_getMaxScrollH),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("maxScrollV","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_getMaxScrollV),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("bottomScrollV","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_getBottomScrollV),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("restrict","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_getRestrict),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("restrict","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_setRestrict),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("textInteractionMode","",Class<IFunction>::getFunction(c->getSystemState(),TextField::_getTextInteractionMode),GETTER_METHOD,true);

	REGISTER_GETTER_SETTER(c, alwaysShowSelection);
	REGISTER_GETTER_SETTER(c, background);
	REGISTER_GETTER_SETTER(c, backgroundColor);
	REGISTER_GETTER_SETTER(c, border);
	REGISTER_GETTER_SETTER(c, borderColor);
	REGISTER_GETTER(c, caretIndex);
	REGISTER_GETTER_SETTER(c, condenseWhite);
	REGISTER_GETTER_SETTER(c, displayAsPassword);
	REGISTER_GETTER_SETTER(c, embedFonts);
	REGISTER_GETTER_SETTER(c, maxChars);
	REGISTER_GETTER_SETTER(c, multiline);
	REGISTER_GETTER_SETTER(c, mouseWheelEnabled);
	REGISTER_GETTER_SETTER(c, scrollH);
	REGISTER_GETTER_SETTER(c, scrollV);
	REGISTER_GETTER_SETTER(c, selectable);
	REGISTER_GETTER(c, selectionBeginIndex);
	REGISTER_GETTER(c, selectionEndIndex);
	REGISTER_GETTER_SETTER(c, sharpness);
	REGISTER_GETTER_SETTER(c, styleSheet);
	REGISTER_GETTER_SETTER(c, textColor);
	REGISTER_GETTER_SETTER(c, thickness);
	REGISTER_GETTER_SETTER(c, type);
	REGISTER_GETTER_SETTER(c, useRichTextClipboard);
}

ASFUNCTIONBODY_GETTER_SETTER(TextField, alwaysShowSelection); // stub
ASFUNCTIONBODY_GETTER_SETTER(TextField, background);
ASFUNCTIONBODY_GETTER_SETTER(TextField, backgroundColor);
ASFUNCTIONBODY_GETTER_SETTER(TextField, border);
ASFUNCTIONBODY_GETTER_SETTER(TextField, borderColor);
ASFUNCTIONBODY_GETTER(TextField, caretIndex);
ASFUNCTIONBODY_GETTER_SETTER(TextField, condenseWhite);
ASFUNCTIONBODY_GETTER_SETTER(TextField, displayAsPassword); // stub
ASFUNCTIONBODY_GETTER_SETTER(TextField, embedFonts); // stub
ASFUNCTIONBODY_GETTER_SETTER(TextField, maxChars); // stub
ASFUNCTIONBODY_GETTER_SETTER(TextField, multiline);
ASFUNCTIONBODY_GETTER_SETTER(TextField, mouseWheelEnabled); // stub
ASFUNCTIONBODY_GETTER_SETTER_CB(TextField, scrollH, validateScrollH);
ASFUNCTIONBODY_GETTER_SETTER_CB(TextField, scrollV, validateScrollV);
ASFUNCTIONBODY_GETTER_SETTER(TextField, selectable); // stub
ASFUNCTIONBODY_GETTER(TextField, selectionBeginIndex);
ASFUNCTIONBODY_GETTER(TextField, selectionEndIndex);
ASFUNCTIONBODY_GETTER_SETTER_CB(TextField, sharpness, validateSharpness); // stub
ASFUNCTIONBODY_GETTER_SETTER(TextField, styleSheet); // stub
ASFUNCTIONBODY_GETTER_SETTER(TextField, textColor);
ASFUNCTIONBODY_GETTER_SETTER_CB(TextField, thickness, validateThickness); // stub
ASFUNCTIONBODY_GETTER_SETTER(TextField, useRichTextClipboard); // stub

void TextField::finalize()
{
	ASObject::finalize();
	restrictChars.reset();
	styleSheet.reset();
}

void TextField::buildTraits(ASObject* o)
{
}

bool TextField::boundsRect(number_t& xmin, number_t& xmax, number_t& ymin, number_t& ymax) const
{
	xmin=0;
	xmax=width;
	ymin=0;
	ymax=height;
	return true;
}

_NR<DisplayObject> TextField::hitTestImpl(_NR<DisplayObject> last, number_t x, number_t y, DisplayObject::HIT_TYPE type)
{
	/* I suppose one does not have to actually hit a character */
	number_t xmin,xmax,ymin,ymax;
	boundsRect(xmin,xmax,ymin,ymax);
	if( xmin <= x && x <= xmax && ymin <= y && y <= ymax && isHittable(type))
	{
		incRef();
		return _MNR(this);
	}
	else
		return NullRef;
}

ASFUNCTIONBODY_ATOM(TextField,_getWordWrap)
{
	TextField* th=obj.as<TextField>();
	ret.setBool(th->wordWrap);
}

ASFUNCTIONBODY_ATOM(TextField,_setWordWrap)
{
	TextField* th=obj.as<TextField>();
	ARG_UNPACK_ATOM(th->wordWrap);
	th->setSizeAndPositionFromAutoSize();
	th->hasChanged=true;
	if(th->onStage && th->isVisible())
		th->requestInvalidation(th->getSystemState());
}

ASFUNCTIONBODY_ATOM(TextField,_getAutoSize)
{
	TextField* th=obj.as<TextField>();
	switch(th->autoSize)
	{
		case AS_NONE:
			ret = asAtom::fromString(sys,"none");
			return;
		case AS_LEFT:
			ret = asAtom::fromString(sys,"left");
			return;
		case AS_RIGHT:
			ret =asAtom::fromString(sys,"right");
			return;
		case AS_CENTER:
			ret = asAtom::fromString(sys,"center");
			return;
	}
}

ASFUNCTIONBODY_ATOM(TextField,_setAutoSize)
{
	TextField* th=obj.as<TextField>();
	tiny_string autoSizeString;
	ARG_UNPACK_ATOM(autoSizeString);

	AUTO_SIZE newAutoSize = AS_NONE;
	if(autoSizeString == "none")
		newAutoSize = AS_NONE;
	else if (autoSizeString == "left")
		newAutoSize = AS_LEFT;
	else if (autoSizeString == "right")
		newAutoSize = AS_RIGHT;
	else if (autoSizeString == "center")
		newAutoSize = AS_CENTER;
	else
		throwError<ArgumentError>(kInvalidEnumError, "autoSize");

	if (th->autoSize != newAutoSize)
	{
		th->autoSize = newAutoSize;
		th->setSizeAndPositionFromAutoSize();
		th->hasChanged=true;
		if(th->onStage && th->isVisible())
			th->requestInvalidation(th->getSystemState());
	}
}

void TextField::setSizeAndPositionFromAutoSize()
{
	if (autoSize == AS_NONE)
		return;

	if (!wordWrap)
	{
		if (autoSize == AS_RIGHT)
			tx += (int)width-(int)textWidth;
		else if (autoSize == AS_CENTER)
			tx += ((int)width - (int)textWidth)/2.0;
		if (width < textWidth)
			width = textWidth;
		if (height < textHeight)
			height = textHeight;
	}
}

ASFUNCTIONBODY_ATOM(TextField,_getWidth)
{
	TextField* th=obj.as<TextField>();
	ret.setUInt(th->width);
}

ASFUNCTIONBODY_ATOM(TextField,_setWidth)
{
	TextField* th=obj.as<TextField>();
	assert_and_throw(argslen==1);
	//The width needs to be updated only if autoSize is off or wordWrap is on TODO:check this, adobe's behavior is not clear
	if(((th->autoSize == AS_NONE)||(th->wordWrap == true))
			&& (th->width != args[0].toUInt()))
	{
		th->width=args[0].toUInt();
		th->hasChanged=true;
		if(th->onStage && th->isVisible())
			th->requestInvalidation(sys);
		else
			th->updateSizes();
	}
}

ASFUNCTIONBODY_ATOM(TextField,_getHeight)
{
	TextField* th=obj.as<TextField>();
	ret.setUInt(th->height);
}

ASFUNCTIONBODY_ATOM(TextField,_setHeight)
{
	TextField* th=obj.as<TextField>();
	assert_and_throw(argslen==1);
	if((th->autoSize == AS_NONE)
		&& (th->height != args[0].toUInt()))
	{
		th->height=args[0].toUInt();
		th->hasChanged=true;
		if(th->onStage && th->isVisible())
			th->requestInvalidation(th->getSystemState());
		else
			th->updateSizes();
	}
	//else do nothing as the height is determined by autoSize
}

ASFUNCTIONBODY_ATOM(TextField,_getTextWidth)
{
	TextField* th=obj.as<TextField>();
	ret.setUInt(th->textWidth);
}

ASFUNCTIONBODY_ATOM(TextField,_getTextHeight)
{
	TextField* th=obj.as<TextField>();
	ret.setUInt(th->textHeight);
}

ASFUNCTIONBODY_ATOM(TextField,_getHtmlText)
{
	TextField* th=obj.as<TextField>();
	ret = asAtom::fromObject(abstract_s(sys,th->toHtmlText()));
}

ASFUNCTIONBODY_ATOM(TextField,_setHtmlText)
{
	TextField* th=obj.as<TextField>();
	tiny_string value;
	ARG_UNPACK_ATOM(value);
	th->setHtmlText(value);
}

ASFUNCTIONBODY_ATOM(TextField,_getText)
{
	TextField* th=obj.as<TextField>();
	ret = asAtom::fromObject(abstract_s(sys,th->text));
}

ASFUNCTIONBODY_ATOM(TextField,_setText)
{
	TextField* th=obj.as<TextField>();
	assert_and_throw(argslen==1);
	th->updateText(args[0].toString(sys));
}

ASFUNCTIONBODY_ATOM(TextField, appendText)
{
	TextField* th=obj.as<TextField>();
	assert_and_throw(argslen==1);
	th->updateText(th->text + args[0].toString(sys));
}

ASFUNCTIONBODY_ATOM(TextField,_getTextFormat)
{
	TextField* th=obj.as<TextField>();
	TextFormat *format=Class<TextFormat>::getInstanceS(sys);

	format->color= asAtom(th->textColor.toUInt());
	format->font = th->font;
	format->size = th->fontSize;

	LOG(LOG_NOT_IMPLEMENTED, "getTextFormat is not fully implemeted");

	ret = asAtom::fromObject(format);
}

ASFUNCTIONBODY_ATOM(TextField,_setTextFormat)
{
	TextField* th=obj.as<TextField>();
	_NR<TextFormat> tf;
	int beginIndex;
	int endIndex;

	ARG_UNPACK_ATOM(tf)(beginIndex, -1)(endIndex, -1);

	if(beginIndex!=-1 || endIndex!=-1)
		LOG(LOG_NOT_IMPLEMENTED,"setTextFormat with beginIndex or endIndex");

	if(tf->color.type != T_NULL)
		th->textColor = tf->color.toUInt();
	if (tf->font != "")
	{
		th->font = tf->font;
		th->fontID = UINT32_MAX;
	}
	th->fontSize = tf->size;

	LOG(LOG_NOT_IMPLEMENTED,"setTextFormat does not read all fields of TextFormat");
}

ASFUNCTIONBODY_ATOM(TextField,_getDefaultTextFormat)
{
	TextField* th=obj.as<TextField>();
	
	TextFormat* tf = Class<TextFormat>::getInstanceS(sys);
	tf->font = th->font;
	LOG(LOG_NOT_IMPLEMENTED,"getDefaultTextFormat does not get all fields of TextFormat");
	ret = asAtom::fromObject(tf);
}

ASFUNCTIONBODY_ATOM(TextField,_setDefaultTextFormat)
{
	TextField* th=obj.as<TextField>();
	_NR<TextFormat> tf;

	ARG_UNPACK_ATOM(tf);

	if(tf->color.type != T_NULL)
		th->textColor = tf->color.toUInt();
	if (tf->font != "")
	{
		th->font = tf->font;
		th->fontID = UINT32_MAX;
	}
	th->fontSize = tf->size;
	LOG(LOG_NOT_IMPLEMENTED,"setDefaultTextFormat does not set all fields of TextFormat");
}

ASFUNCTIONBODY_ATOM(TextField, _getter_type)
{
	TextField* th=obj.as<TextField>();
	if (th->type == ET_READ_ONLY)
		ret = asAtom::fromString(sys,"dynamic");
	else
		ret = asAtom::fromString(sys,"input");
}

ASFUNCTIONBODY_ATOM(TextField, _setter_type)
{
	TextField* th=obj.as<TextField>();

	tiny_string value;
	ARG_UNPACK_ATOM(value);

	if (value == "dynamic")
		th->type = ET_READ_ONLY;
	else if (value == "input")
		th->type = ET_EDITABLE;
	else
		throwError<ArgumentError>(kInvalidEnumError, "type");
}

ASFUNCTIONBODY_ATOM(TextField,_getLineIndexAtPoint)
{
	TextField* th=obj.as<TextField>();
	number_t x;
	number_t y;
	ARG_UNPACK_ATOM(x) (y);

	std::vector<LineData> lines = CairoPangoRenderer::getLineData(*th);
	std::vector<LineData>::const_iterator it;
	int i;
	for (i=0, it=lines.begin(); it!=lines.end(); ++i, ++it)
	{
		if (x > it->extents.Xmin && x <= it->extents.Xmax &&
		    y > it->extents.Ymin && y <= it->extents.Ymax)
		{
			ret.setInt(i);
			return;
		}
	}

	ret.setInt(-1);
}

ASFUNCTIONBODY_ATOM(TextField,_getLineIndexOfChar)
{
	TextField* th=obj.as<TextField>();
	int32_t charIndex;
	ARG_UNPACK_ATOM(charIndex);

	if (charIndex < 0)
	{
		ret.setInt(-1);
		return;
	}

	std::vector<LineData> lines = CairoPangoRenderer::getLineData(*th);
	std::vector<LineData>::const_iterator it;
	int i;
	for (i=0, it=lines.begin(); it!=lines.end(); ++i, ++it)
	{
		if (charIndex >= it->firstCharOffset &&
		    charIndex < it->firstCharOffset + it->length)
		{
			ret.setInt(i);
			return;
		}
	}

	// testing shows that returns -1 on invalid index instead of
	// throwing RangeError
	ret.setInt(-1);
}

ASFUNCTIONBODY_ATOM(TextField,_getLineLength)
{
	TextField* th=obj.as<TextField>();
	int32_t  lineIndex;
	ARG_UNPACK_ATOM(lineIndex);

	std::vector<LineData> lines = CairoPangoRenderer::getLineData(*th);
	if (lineIndex < 0 || lineIndex >= (int32_t)lines.size())
		throwError<RangeError>(kParamRangeError);

	ret.setInt(lines[lineIndex].length);
}

ASFUNCTIONBODY_ATOM(TextField,_getLineMetrics)
{
	TextField* th=obj.as<TextField>();
	int32_t  lineIndex;
	ARG_UNPACK_ATOM(lineIndex);

	std::vector<LineData> lines = CairoPangoRenderer::getLineData(*th);
	if (lineIndex < 0 || lineIndex >= (int32_t)lines.size())
		throwError<RangeError>(kParamRangeError);

	ret = asAtom::fromObject(Class<TextLineMetrics>::getInstanceS(sys,
		lines[lineIndex].indent,
		lines[lineIndex].extents.Xmax - lines[lineIndex].extents.Xmin,
		lines[lineIndex].extents.Ymax - lines[lineIndex].extents.Ymin,
		lines[lineIndex].ascent,
		lines[lineIndex].descent,
		lines[lineIndex].leading));
}

ASFUNCTIONBODY_ATOM(TextField,_getLineOffset)
{
	TextField* th=obj.as<TextField>();
	int32_t  lineIndex;
	ARG_UNPACK_ATOM(lineIndex);

	std::vector<LineData> lines = CairoPangoRenderer::getLineData(*th);
	if (lineIndex < 0 || lineIndex >= (int32_t)lines.size())
		throwError<RangeError>(kParamRangeError);

	ret.setInt(lines[lineIndex].firstCharOffset);
}

ASFUNCTIONBODY_ATOM(TextField,_getLineText)
{
	TextField* th=obj.as<TextField>();
	int32_t  lineIndex;
	ARG_UNPACK_ATOM(lineIndex);

	std::vector<LineData> lines = CairoPangoRenderer::getLineData(*th);
	if (lineIndex < 0 || lineIndex >= (int32_t)lines.size())
		throwError<RangeError>(kParamRangeError);

	tiny_string substr = th->text.substr(lines[lineIndex].firstCharOffset,
					     lines[lineIndex].length);
	ret = asAtom::fromObject(abstract_s(sys,substr));
}

ASFUNCTIONBODY_ATOM(TextField,_getAntiAliasType)
{
	TextField* th=obj.as<TextField>();
	if (th->antiAliasType == AA_NORMAL)
		ret = asAtom::fromString(sys,"normal");
	else
		ret = asAtom::fromString(sys,"advanced");
}

ASFUNCTIONBODY_ATOM(TextField,_setAntiAliasType)
{
	TextField* th=obj.as<TextField>();
	tiny_string value;
	ARG_UNPACK_ATOM(value);

	if (value == "advanced")
	{
		th->antiAliasType = AA_ADVANCED;
		LOG(LOG_NOT_IMPLEMENTED, "TextField advanced antiAliasType not implemented");
	}
	else
		th->antiAliasType = AA_NORMAL;
}

ASFUNCTIONBODY_ATOM(TextField,_getGridFitType)
{
	TextField* th=obj.as<TextField>();
	if (th->gridFitType == GF_NONE)
		ret = asAtom::fromString(sys,"none");
	else if (th->gridFitType == GF_PIXEL)
		ret = asAtom::fromString(sys,"pixel");
	else
		ret = asAtom::fromString(sys,"subpixel");
}

ASFUNCTIONBODY_ATOM(TextField,_setGridFitType)
{
	TextField* th=obj.as<TextField>();
	tiny_string value;
	ARG_UNPACK_ATOM(value);

	if (value == "none")
		th->gridFitType = GF_NONE;
	else if (value == "pixel")
		th->gridFitType = GF_PIXEL;
	else
		th->gridFitType = GF_SUBPIXEL;

	LOG(LOG_NOT_IMPLEMENTED, "TextField gridFitType not implemented");

}

ASFUNCTIONBODY_ATOM(TextField,_getLength)
{
	TextField* th=obj.as<TextField>();
	ret.setUInt(th->text.numChars());
}

ASFUNCTIONBODY_ATOM(TextField,_getNumLines)
{
	TextField* th=obj.as<TextField>();
	ret.setInt((int32_t)CairoPangoRenderer::getLineData(*th).size());
}

ASFUNCTIONBODY_ATOM(TextField,_getMaxScrollH)
{
	TextField* th=obj.as<TextField>();
	ret.setInt(th->getMaxScrollH());
}

ASFUNCTIONBODY_ATOM(TextField,_getMaxScrollV)
{
	TextField* th=obj.as<TextField>();
	ret.setInt(th->getMaxScrollV());
}

ASFUNCTIONBODY_ATOM(TextField,_getBottomScrollV)
{
	TextField* th=obj.as<TextField>();
	std::vector<LineData> lines = CairoPangoRenderer::getLineData(*th);
	for (unsigned int k=0; k<lines.size()-1; k++)
	{
		if (lines[k+1].extents.Ymin >= (int)th->height)
		{
			ret.setInt((int32_t)k + 1);
			return;
		}
	}

	ret.setInt((int32_t)lines.size() + 1);
}

ASFUNCTIONBODY_ATOM(TextField,_getRestrict)
{
	TextField* th=obj.as<TextField>();
	if (th->restrictChars.isNull())
		return;
	else
	{
		th->restrictChars->incRef();
		ret = asAtom::fromObject(th->restrictChars.getPtr());
	}
}

ASFUNCTIONBODY_ATOM(TextField,_setRestrict)
{
	TextField* th=obj.as<TextField>();
	ARG_UNPACK_ATOM(th->restrictChars);
	if (!th->restrictChars.isNull())
		LOG(LOG_NOT_IMPLEMENTED, "TextField restrict property");
}

ASFUNCTIONBODY_ATOM(TextField,_getTextInteractionMode)
{
	TextField* th=obj.as<TextField>();
	if (th->textInteractionMode == TI_NORMAL)
		ret = asAtom::fromString(sys,"normal");
	else
		ret = asAtom::fromString(sys,"selection");
}

ASFUNCTIONBODY_ATOM(TextField,_setSelection)
{
	TextField* th=obj.as<TextField>();
	ARG_UNPACK_ATOM(th->selectionBeginIndex) (th->selectionEndIndex);

	if (th->selectionBeginIndex < 0)
		th->selectionBeginIndex = 0;

	if (th->selectionEndIndex >= (int32_t)th->text.numChars())
		th->selectionEndIndex = th->text.numChars()-1;

	if (th->selectionBeginIndex > th->selectionEndIndex)
		th->selectionBeginIndex = th->selectionEndIndex;

	if (th->selectionBeginIndex == th->selectionEndIndex)
		th->caretIndex = th->selectionBeginIndex;

	LOG(LOG_NOT_IMPLEMENTED, "TextField selection will not be rendered");
}

ASFUNCTIONBODY_ATOM(TextField,_replaceSelectedText)
{
	TextField* th=obj.as<TextField>();
	tiny_string newText;
	ARG_UNPACK_ATOM(newText);
	th->replaceText(th->selectionBeginIndex, th->selectionEndIndex, newText);
}

ASFUNCTIONBODY_ATOM(TextField,_replaceText)
{
	TextField* th=obj.as<TextField>();
	int32_t begin;
	int32_t end;
	tiny_string newText;
	ARG_UNPACK_ATOM(begin) (end) (newText);
	th->replaceText(begin, end, newText);
}

void TextField::replaceText(unsigned int begin, unsigned int end, const tiny_string& newText)
{
	if (!styleSheet.isNull())
		throw Class<ASError>::getInstanceS(getSystemState(),"Can not replace text on text field with a style sheet");

	if (begin >= text.numChars())
	{
		text = text + newText;
	}
	else if (begin > end)
	{
		return;
	}
	else if (end >= text.numChars())
	{
		text = text.substr(0, begin) + newText;
	}
	else
	{
		text = text.substr(0, begin) + newText + text.substr(end, text.end());
	}

	textUpdated();
}

void TextField::validateThickness(number_t /*oldValue*/)
{
	thickness = dmin(dmax(thickness, -200.), 200.);
}

void TextField::validateSharpness(number_t /*oldValue*/)
{
	sharpness = dmin(dmax(sharpness, -400.), 400.);
}

void TextField::validateScrollH(int32_t oldValue)
{
	int32_t maxScrollH = getMaxScrollH();
	if (scrollH > maxScrollH)
		scrollH = maxScrollH;
	hasChanged=true;

	if (onStage && (scrollH != oldValue) && isVisible())
		requestInvalidation(this->getSystemState());
}

void TextField::validateScrollV(int32_t oldValue)
{
	int32_t maxScrollV = getMaxScrollV();
	if (scrollV < 1)
		scrollV = 1;
	else if (scrollV > maxScrollV)
		scrollV = maxScrollV;
	hasChanged=true;

	if (onStage && (scrollV != oldValue) && isVisible())
		requestInvalidation(this->getSystemState());
}

int32_t TextField::getMaxScrollH()
{
	if (wordWrap || (textWidth <= width))
		return 0;
	else
		return textWidth;
}

int32_t TextField::getMaxScrollV()
{
	std::vector<LineData> lines = CairoPangoRenderer::getLineData(*this);
	if (lines.size() <= 1)
		return 1;

	int32_t Ymax = lines[lines.size()-1].extents.Ymax;
	int32_t measuredTextHeight = Ymax - lines[0].extents.Ymin;
	if (measuredTextHeight <= (int32_t)height)
		return 1;

	// one full page from the bottom
	for (int k=(int)lines.size()-1; k>=0; k--)
	{
		if (Ymax - lines[k].extents.Ymin > (int32_t)height)
		{
			return imin(k+1+1, lines.size());
		}
	}

	return 1;
}

void TextField::updateSizes()
{
	uint32_t w,h,tw,th;
	w = width;
	h = height;
	tw = textWidth;
	th = textHeight;
	
	//Compute (text)width, (text)height
	
	RootMovieClip* currentRoot=getSystemState()->mainClip;
	DefineFont3Tag* embeddedfont = (fontID != UINT32_MAX ? currentRoot->getEmbeddedFontByID(fontID) : currentRoot->getEmbeddedFont(font));
	if (embeddedfont)
	{
		tokens.clear();
		scaling = 1.0f/1024.0f/20.0f;
		embeddedfont->fillTextTokens(tokens,text,fontSize,textColor);
		number_t x1,x2,y1,y2;
		if (TokenContainer::boundsRect(x1,x2,y1,y2))
		{
			tw = x2-x1;
			th = y2-y1;
		}
	}
	else
		CairoPangoRenderer::getBounds(*this, w, h, tw, th);
	//width = w; //TODO: check the case when w,h == 0
	textWidth=tw;
	//height = h;
	textHeight=th;
}

tiny_string TextField::toHtmlText()
{
	pugi::xml_document doc;
	pugi::xml_node root = doc.append_child("font");

	ostringstream ss;
	ss << fontSize;
	root.append_attribute("size").set_value(ss.str().c_str());
	root.append_attribute("color").set_value(textColor.toString().raw_buf());
	root.append_attribute("face").set_value(font.raw_buf());

	//Split text into paragraphs and wraps them into <p> tags
	uint32_t para_start = 0;
	uint32_t para_end;
	do
	{
		para_end = text.find("\n", para_start);
		if (para_end == text.npos)
			para_end = text.numChars();

		root.append_child("p").set_value(text.substr(para_start, para_end).raw_buf());
		para_start = para_end + 1;
	} while (para_end < text.numChars());

	ostringstream buf;
	doc.print(buf);
	tiny_string ret = tiny_string(buf.str());
	return ret;
}

void TextField::setHtmlText(const tiny_string& html)
{
	HtmlTextParser parser;
	if (condenseWhite)
	{
		tiny_string processedHtml = compactHTMLWhiteSpace(html);
		parser.parseTextAndFormating(processedHtml, this);
	}
	else
	{
		parser.parseTextAndFormating(html, this);
	}
	updateSizes();
	hasChanged=true;
	textUpdated();
}

tiny_string TextField::compactHTMLWhiteSpace(const tiny_string& html)
{
	tiny_string compacted;
	bool previousWasSpace = false;
	for (CharIterator ch=html.begin(); ch!=html.end(); ++ch)
	{
		if (g_unichar_isspace(*ch))
		{
			if (!previousWasSpace)
				compacted += ' ';
			previousWasSpace = true;
		}
		else
		{
			compacted += *ch;
			previousWasSpace = false;
		}
	}

	return compacted;
}

void TextField::updateText(const tiny_string& new_text)
{
	if (text == new_text)
		return;
	text = new_text;
	textUpdated();
}

void TextField::textUpdated()
{
	scrollH = 0;
	scrollV = 1;
	selectionBeginIndex = 0;
	selectionEndIndex = 0;
	updateSizes();
	setSizeAndPositionFromAutoSize();
	hasChanged=true;

	if(onStage && isVisible())
		requestInvalidation(this->getSystemState());
}

void TextField::requestInvalidation(InvalidateQueue* q)
{
	if (!tokensEmpty())
		TokenContainer::requestInvalidation(q);
	else
	{
		incRef();
		updateSizes();
		q->addToInvalidateQueue(_MR(this));
	}
}

IDrawable* TextField::invalidate(DisplayObject* target, const MATRIX& initialMatrix,bool smoothing)
{
	int32_t x,y;
	uint32_t width,height;
	number_t bxmin,bxmax,bymin,bymax;
	if(boundsRect(bxmin,bxmax,bymin,bymax)==false)
	{
		//No contents, nothing to do
		return NULL;
	}

	RootMovieClip* currentRoot=getSystemState()->mainClip;
	DefineFont3Tag* embeddedfont = (fontID != UINT32_MAX ? currentRoot->getEmbeddedFontByID(fontID) : currentRoot->getEmbeddedFont(font));
	tokens.clear();
	if (embeddedfont)
	{
		scaling = 1.0f/1024.0f/20.0f;
		embeddedfont->fillTextTokens(tokens,text,fontSize,textColor);
	}
	if (!tokensEmpty())
		return TokenContainer::invalidate(target, initialMatrix,smoothing);
	
	MATRIX totalMatrix;
	std::vector<IDrawable::MaskData> masks;
	computeMasksAndMatrix(target, masks, totalMatrix);
	totalMatrix=initialMatrix.multiplyMatrix(totalMatrix);
	computeBoundsForTransformedRect(bxmin,bxmax,bymin,bymax,x,y,width,height,totalMatrix);
	if(width==0 || height==0)
		return NULL;
	if(totalMatrix.getScaleX() != 1 || totalMatrix.getScaleY() != 1)
		LOG(LOG_NOT_IMPLEMENTED, "TextField when scaled is not correctly implemented");
	// use specialized Renderer from EngineData, if available, otherwise fallback to Pango
	IDrawable* res = this->getSystemState()->getEngineData()->getTextRenderDrawable(*this,totalMatrix, x, y, width, height, 1.0f,getConcatenatedAlpha(), masks,smoothing);
	if (res != NULL)
		return res;
	/**  TODO: The scaling is done differently for textfields : height changes are applied directly
		on the font size. In some cases, it can change the width (if autosize is on and wordwrap off).
		Width changes do not change the font size, and do nothing when autosize is on and wordwrap off.
		Currently, the TextField is stretched in case of scaling.
	*/
	return new CairoPangoRenderer(*this,
				totalMatrix, x, y, width, height, 1.0f,
				getConcatenatedAlpha(), masks,smoothing);
}

void TextField::renderImpl(RenderContext& ctxt) const
{
	defaultRender(ctxt);
}

void TextField::HtmlTextParser::parseTextAndFormating(const tiny_string& html,
						      TextData *dest)
{
	textdata = dest;
	if (!textdata)
		return;

	textdata->text = "";

	tiny_string rooted = tiny_string("<root>") + html + tiny_string("</root>");
	pugi::xml_document doc;
	if (doc.load_buffer(rooted.raw_buf(),rooted.numBytes()).status == pugi::status_ok)
	{
		doc.traverse(*this);
	}
	else
	{
		LOG(LOG_ERROR, "TextField HTML parser error");
		return;
	}
}

bool TextField::HtmlTextParser::for_each(pugi::xml_node &node)
{

	if (!textdata)
		return true;
	tiny_string name = node.name();
	textdata->text += node.value();
	if (name == "br")
	{
		if (textdata->multiline)
			textdata->text += "\n";
			
	}
	else if (name == "p")
	{
		if (textdata->multiline)
		{
			if (!textdata->text.empty() && 
			    !textdata->text.endsWith("\n"))
				textdata->text += "\n";
		}
		for (auto it=node.attributes_begin(); it!=node.attributes_end(); ++it)
		{
			tiny_string attrname = it->name();
			tiny_string value = it->value();
			if (attrname == "align")
			{
				if (value == "left")
					textdata->autoSize = TextData::AS_LEFT;
				if (value == "center")
					textdata->autoSize = TextData::AS_CENTER;
				if (value == "right")
					textdata->autoSize = TextData::AS_RIGHT;
			}
			else
			{
				LOG(LOG_NOT_IMPLEMENTED,"TextField html tag <p>: unsupported attribute:"<<attrname);
			}
		}

	}
	else if (name == "font")
	{
//		if (!textdata->text.empty())
//		{
//			LOG(LOG_NOT_IMPLEMENTED, "Font can be defined only in the beginning");
//			return false;
//		}

		for (auto it=node.attributes_begin(); it!=node.attributes_end(); ++it)
		{
			tiny_string attrname = it->name();
			if (attrname == "face")
			{
				textdata->font = it->value();
				textdata->fontID = UINT32_MAX;
				
			}
			else if (attrname == "size")
			{
				textdata->fontSize = parseFontSize(it->value(), textdata->fontSize);
			}
			else if (attrname == "color")
			{
				textdata->textColor = RGB(tiny_string(it->value()));
			}
			else
				LOG(LOG_NOT_IMPLEMENTED,"TextField html tag <font>: unsupported attribute:"<<attrname<<" "<<it->value());
		}
	}
	else if (name == "" || name == "root" || name == "body")
		return true;
	else if (name == "a" || name == "img" || name == "u" ||
		 name == "li" || name == "b" || name == "i" ||
		 name == "span" || name == "textformat" || name == "tab")
	{
		LOG(LOG_NOT_IMPLEMENTED, _("Unsupported tag in TextField: ") << name);
	}
	else
	{
		LOG(LOG_NOT_IMPLEMENTED, _("Unknown tag in TextField: ") << name);
	}
	return true;
}

uint32_t TextField::HtmlTextParser::parseFontSize(const Glib::ustring& sizestr,
						  uint32_t currentFontSize)
{
	const char *s = sizestr.c_str();
	if (!s)
		return currentFontSize;

	uint32_t basesize = 0;
	int multiplier = 1;
	if (s[0] == '+' || s[0] == '-')
	{
		// relative size
		basesize = currentFontSize;
		if (s[0] == '-')
			multiplier = -1;
	}

	int64_t size = basesize + multiplier*g_ascii_strtoll(s, NULL, 10);
	if (size < 1)
		size = 1;
	if (size > G_MAXUINT32)
		size = G_MAXUINT32;
	
	return (uint32_t)size;
}

void TextFieldAutoSize::sinit(Class_base* c)
{
	CLASS_SETUP_NO_CONSTRUCTOR(c, ASObject, CLASS_FINAL | CLASS_SEALED);
	c->setVariableAtomByQName("CENTER",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"center"),CONSTANT_TRAIT);
	c->setVariableAtomByQName("LEFT",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"left"),CONSTANT_TRAIT);
	c->setVariableAtomByQName("NONE",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"none"),CONSTANT_TRAIT);
	c->setVariableAtomByQName("RIGHT",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"right"),CONSTANT_TRAIT);
}

void TextFieldType::sinit(Class_base* c)
{
	CLASS_SETUP_NO_CONSTRUCTOR(c, ASObject, CLASS_FINAL | CLASS_SEALED);
	c->setVariableAtomByQName("DYNAMIC",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"dynamic"),CONSTANT_TRAIT);
	c->setVariableAtomByQName("INPUT",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"input"),CONSTANT_TRAIT);
}

void TextFormatAlign::sinit(Class_base* c)
{
	CLASS_SETUP_NO_CONSTRUCTOR(c, ASObject, CLASS_FINAL | CLASS_SEALED);
	c->isReusable = true;
	c->setVariableAtomByQName("CENTER",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"center"),CONSTANT_TRAIT);
	c->setVariableAtomByQName("END",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"end"),CONSTANT_TRAIT);
	c->setVariableAtomByQName("JUSTIFY",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"justify"),CONSTANT_TRAIT);
	c->setVariableAtomByQName("LEFT",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"left"),CONSTANT_TRAIT);
	c->setVariableAtomByQName("RIGHT",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"right"),CONSTANT_TRAIT);
	c->setVariableAtomByQName("START",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"start"),CONSTANT_TRAIT);
}

void TextFormat::sinit(Class_base* c)
{
	CLASS_SETUP(c, ASObject, _constructor, CLASS_SEALED);
	REGISTER_GETTER_SETTER(c,align);
	REGISTER_GETTER_SETTER(c,blockIndent);
	REGISTER_GETTER_SETTER(c,bold);
	REGISTER_GETTER_SETTER(c,bullet);
	REGISTER_GETTER_SETTER(c,color);
	REGISTER_GETTER_SETTER(c,font);
	REGISTER_GETTER_SETTER(c,indent);
	REGISTER_GETTER_SETTER(c,italic);
	REGISTER_GETTER_SETTER(c,kerning);
	REGISTER_GETTER_SETTER(c,leading);
	REGISTER_GETTER_SETTER(c,leftMargin);
	REGISTER_GETTER_SETTER(c,letterSpacing);
	REGISTER_GETTER_SETTER(c,rightMargin);
	REGISTER_GETTER_SETTER(c,size);
	REGISTER_GETTER_SETTER(c,tabStops);
	REGISTER_GETTER_SETTER(c,target);
	REGISTER_GETTER_SETTER(c,underline);
	REGISTER_GETTER_SETTER(c,url);
	// not in spec, but available in adobe player
	REGISTER_GETTER_SETTER(c,display);
}

bool TextFormat::destruct()
{
	blockIndent = asAtom::nullAtom;
	bold = asAtom::nullAtom;
	bullet = asAtom::nullAtom;
	color = asAtom::nullAtom;
	indent = asAtom::nullAtom;
	italic = asAtom::nullAtom;
	kerning = asAtom::nullAtom;
	leading = asAtom::nullAtom;
	leftMargin = asAtom::nullAtom;
	letterSpacing = asAtom::nullAtom;
	rightMargin = asAtom::nullAtom;
	tabStops.reset();
	underline = asAtom::nullAtom;
	display = "";
	align="";
	url="";
	target="";
	font="";
	size=12;
	return ASObject::destruct();
}

ASFUNCTIONBODY_ATOM(TextFormat,_constructor)
{
	TextFormat* th=obj.as<TextFormat>();
	ARG_UNPACK_ATOM (th->font, "")
		(th->size, 12)
		(th->color,asAtom::nullAtom)
		(th->bold,asAtom::nullAtom)
		(th->italic,asAtom::nullAtom)
		(th->underline,asAtom::nullAtom)
		(th->url,"")
		(th->target,"")
		(th->align,"left")
		(th->leftMargin,asAtom::nullAtom)
		(th->rightMargin,asAtom::nullAtom)
		(th->indent,asAtom::nullAtom)
		(th->leading,asAtom::nullAtom);
}

ASFUNCTIONBODY_GETTER_SETTER_CB(TextFormat,align,onAlign);
ASFUNCTIONBODY_GETTER_SETTER(TextFormat,blockIndent);
ASFUNCTIONBODY_GETTER_SETTER(TextFormat,bold);
ASFUNCTIONBODY_GETTER_SETTER(TextFormat,bullet);
ASFUNCTIONBODY_GETTER_SETTER(TextFormat,color);
ASFUNCTIONBODY_GETTER_SETTER(TextFormat,font);
ASFUNCTIONBODY_GETTER_SETTER(TextFormat,indent);
ASFUNCTIONBODY_GETTER_SETTER(TextFormat,italic);
ASFUNCTIONBODY_GETTER_SETTER(TextFormat,kerning);
ASFUNCTIONBODY_GETTER_SETTER(TextFormat,leading);
ASFUNCTIONBODY_GETTER_SETTER(TextFormat,leftMargin);
ASFUNCTIONBODY_GETTER_SETTER(TextFormat,letterSpacing);
ASFUNCTIONBODY_GETTER_SETTER(TextFormat,rightMargin);
ASFUNCTIONBODY_GETTER_SETTER(TextFormat,size);
ASFUNCTIONBODY_GETTER_SETTER(TextFormat,tabStops);
ASFUNCTIONBODY_GETTER_SETTER(TextFormat,target);
ASFUNCTIONBODY_GETTER_SETTER(TextFormat,underline);
ASFUNCTIONBODY_GETTER_SETTER(TextFormat,url);
ASFUNCTIONBODY_GETTER_SETTER(TextFormat,display);

void TextFormat::buildTraits(ASObject* o)
{
}

void TextFormat::onAlign(const tiny_string& old)
{
	if (align != "" && align != "center" && align != "end" && align != "justify" && 
	    align != "left" && align != "right" && align != "start")
	{
		align = old;
		throwError<ArgumentError>(kInvalidEnumError, "align");
	}
}

void StyleSheet::finalize()
{
	EventDispatcher::finalize();
	styles.clear();
}

void StyleSheet::sinit(Class_base* c)
{
	CLASS_SETUP_NO_CONSTRUCTOR(c, EventDispatcher, CLASS_DYNAMIC_NOT_FINAL);
	c->setDeclaredMethodByQName("styleNames","",Class<IFunction>::getFunction(c->getSystemState(),_getStyleNames),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("setStyle","",Class<IFunction>::getFunction(c->getSystemState(),setStyle),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("getStyle","",Class<IFunction>::getFunction(c->getSystemState(),getStyle),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("parseCSS","",Class<IFunction>::getFunction(c->getSystemState(),parseCSS),NORMAL_METHOD,true);
}

void StyleSheet::buildTraits(ASObject* o)
{
}

ASFUNCTIONBODY_ATOM(StyleSheet,setStyle)
{
	StyleSheet* th=obj.as<StyleSheet>();
	assert_and_throw(argslen==2);
	const tiny_string& arg0=args[0].toString(sys);
	ASATOM_INCREF(args[1]); //TODO: should make a copy, see reference
	map<tiny_string, asAtom>::iterator it=th->styles.find(arg0);
	//NOTE: we cannot use the [] operator as References cannot be non initialized
	if(it!=th->styles.end()) //Style already exists
		it->second=args[1];
	else
		th->styles.insert(make_pair(arg0,args[1]));
}

ASFUNCTIONBODY_ATOM(StyleSheet,getStyle)
{
	StyleSheet* th=obj.as<StyleSheet>();
	assert_and_throw(argslen==1);
	const tiny_string& arg0=args[0].toString(sys);
	map<tiny_string, asAtom>::iterator it=th->styles.find(arg0);
	if(it!=th->styles.end()) //Style already exists
	{
		//TODO: should make a copy, see reference
		ASATOM_INCREF(it->second);
		ret = it->second;
	}
	else
	{
		// Tested behaviour is to return an empty ASObject
		// instead of Null as is said in the documentation
		ret = asAtom::fromObject(Class<ASObject>::getInstanceS(sys));
	}
}

ASFUNCTIONBODY_ATOM(StyleSheet,_getStyleNames)
{
	StyleSheet* th=obj.as<StyleSheet>();
	assert_and_throw(argslen==0);
	Array* res=Class<Array>::getInstanceSNoArgs(sys);
	map<tiny_string, asAtom>::const_iterator it=th->styles.begin();
	for(;it!=th->styles.end();++it)
		res->push(asAtom::fromObject(abstract_s(sys,it->first)));
	ret = asAtom::fromObject(res);
}

ASFUNCTIONBODY_ATOM(StyleSheet,parseCSS)
{
	//StyleSheet* th=obj.as<StyleSheet>();
	tiny_string css;
	ARG_UNPACK_ATOM(css);
	LOG(LOG_NOT_IMPLEMENTED,"StyleSheet.parseCSS does nothing");
}

void StaticText::sinit(Class_base* c)
{
	// FIXME: the constructor should be
	// _constructorNotInstantiatable but that breaks when
	// DisplayObjectContainer::initFrame calls the constructor
	CLASS_SETUP_NO_CONSTRUCTOR(c, DisplayObject, CLASS_FINAL | CLASS_SEALED);
	c->setDeclaredMethodByQName("text","",Class<IFunction>::getFunction(c->getSystemState(),_getText),GETTER_METHOD,true);
}

ASFUNCTIONBODY_ATOM(StaticText,_getText)
{
	LOG(LOG_NOT_IMPLEMENTED,"flash.display.StaticText.text is not implemented");
	ret = asAtom::fromString(sys,"");
}

void FontStyle::sinit(Class_base* c)
{
	CLASS_SETUP_NO_CONSTRUCTOR(c, ASObject, CLASS_FINAL | CLASS_SEALED);
	c->setVariableAtomByQName("BOLD",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"bold"),CONSTANT_TRAIT);
	c->setVariableAtomByQName("BOLD_ITALIC",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"boldItalic"),CONSTANT_TRAIT);
	c->setVariableAtomByQName("ITALIC",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"italic"),CONSTANT_TRAIT);
	c->setVariableAtomByQName("REGULAR",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"regular"),CONSTANT_TRAIT);
}

void FontType::sinit(Class_base* c)
{
	CLASS_SETUP_NO_CONSTRUCTOR(c, ASObject, CLASS_FINAL | CLASS_SEALED);
	c->setVariableAtomByQName("DEVICE",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"device"),CONSTANT_TRAIT);
	c->setVariableAtomByQName("EMBEDDED",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"embedded"),CONSTANT_TRAIT);
	c->setVariableAtomByQName("EMBEDDED_CFF",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"embeddedCFF"),CONSTANT_TRAIT);
}

void TextDisplayMode::sinit(Class_base* c)
{
	CLASS_SETUP_NO_CONSTRUCTOR(c, ASObject, CLASS_FINAL | CLASS_SEALED);
	c->setVariableAtomByQName("CRT",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"crt"),CONSTANT_TRAIT);
	c->setVariableAtomByQName("DEFAULT",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"default"),CONSTANT_TRAIT);
	c->setVariableAtomByQName("LCD",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"lcd"),CONSTANT_TRAIT);
}

void TextColorType::sinit(Class_base* c)
{
	CLASS_SETUP_NO_CONSTRUCTOR(c, ASObject, CLASS_FINAL | CLASS_SEALED);
	c->setVariableAtomByQName("DARK_COLOR",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"dark"),CONSTANT_TRAIT);
	c->setVariableAtomByQName("LIGHT_COLOR",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"light"),CONSTANT_TRAIT);
}

void GridFitType::sinit(Class_base* c)
{
	CLASS_SETUP_NO_CONSTRUCTOR(c, ASObject, CLASS_FINAL | CLASS_SEALED);
	c->setVariableAtomByQName("NONE",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"none"),CONSTANT_TRAIT);
	c->setVariableAtomByQName("PIXEL",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"pixel"),CONSTANT_TRAIT);
	c->setVariableAtomByQName("SUBPIXEL",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"subpixel"),CONSTANT_TRAIT);
}

void TextInteractionMode::sinit(Class_base* c)
{
	CLASS_SETUP(c, ASObject, _constructor, CLASS_FINAL | CLASS_SEALED);
	c->setVariableAtomByQName("NORMAL",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"normal"),CONSTANT_TRAIT);
	c->setVariableAtomByQName("SELECTION",nsNameAndKind(),asAtom::fromString(c->getSystemState(),"selection"),CONSTANT_TRAIT);
}

void TextLineMetrics::sinit(Class_base* c)
{
	CLASS_SETUP(c, ASObject, _constructor, CLASS_SEALED);
	REGISTER_GETTER_SETTER(c, ascent);
	REGISTER_GETTER_SETTER(c, descent);
	REGISTER_GETTER_SETTER(c, height);
	REGISTER_GETTER_SETTER(c, leading);
	REGISTER_GETTER_SETTER(c, width);
	REGISTER_GETTER_SETTER(c, x);
}

ASFUNCTIONBODY_ATOM(TextLineMetrics,_constructor)
{
	if (argslen == 0)
	{
		//Assume that the values were initialized by the C++
		//constructor
		return;
	}

	TextLineMetrics* th=obj.as<TextLineMetrics>();
	ARG_UNPACK_ATOM (th->x) (th->width) (th->height) (th->ascent) (th->descent) (th->leading);
}

ASFUNCTIONBODY_GETTER_SETTER(TextLineMetrics, ascent);
ASFUNCTIONBODY_GETTER_SETTER(TextLineMetrics, descent);
ASFUNCTIONBODY_GETTER_SETTER(TextLineMetrics, height);
ASFUNCTIONBODY_GETTER_SETTER(TextLineMetrics, leading);
ASFUNCTIONBODY_GETTER_SETTER(TextLineMetrics, width);
ASFUNCTIONBODY_GETTER_SETTER(TextLineMetrics, x);
