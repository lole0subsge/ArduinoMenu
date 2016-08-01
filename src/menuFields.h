/********************
www.r-site.net
Nov. 2014 Rui Azevedo - ruihfazevedo(@rrob@)gmail.com
implementing menu fields as options that show a value
value (variable reference) can be changed by either using:
	menuField - for numeric varibles between range and optinally with a tune speed
	menuChoose - Use menu like navigation to select variable value
	menuToggle - cycle list of possible values

class menuValue is used as a menu prompt with an associated value for menuChoose and menuToggle

this classes are implemented as templates to accomodate virtually any value type

creative commons license 3.0: Attribution-ShareAlike CC BY-SA
This software is furnished "as is", without technical support, and with no
warranty, express or implied, as to its usefulness for any purpose.

Thread Safe: No
Extensible: Yes

v2.0 - 	Calling action on every elements

***/

#ifndef RSITE_ARDUINOP_MENU_FIELDS
#define RSITE_ARDUINOP_MENU_FIELDS

	#include "menu.h"

	//prompts holding values for choose and toggle
	//TODO: 'action' is not needed here, we could implement it but its kinda redundant
	//	either implement it or remove it (by forking class derivation earlier)
	//	not that we dont need a function to be called on variable change,
	//	but this function MUST be defined on menu levell, not on setting level
	// rah Nov.2014
  template<typename T>
  class menuValue:public prompt {
	  public:
	  T value;
    inline menuValue(const char * text,T value):prompt(text),value(value) {}
    inline menuValue(const char * text,T value,promptAction action)
    	:prompt(text,action),value(value) {}
  };

	//Prompt linked to a variable
	//TODO: implement Escape on a field cancels the editting (undo) restoring the value
	static const char* numericChars="0123456789.";
	template <typename T>
	class menuField:public menuNode {
	public:
		T& value;
		const char* units;
		T low,high,step,tune;
		bool tunning;
		promptFeedback (*func)();
		char ch;
		T tmp;
		menuField(T &value,const char * text,const char *units,T low,T high,T step,T tune=0,promptFeedback (*func)()=nothing)
			:menuNode(text),value(value),units(units),low(low),high(high),step(step),tune(tune),func(func),tunning(false),ch(0),tmp(value) {}
		virtual bool needRedraw() {return tmp!=value;}
		virtual void printTo(menuOut& p) {
			p.print(text);
			p.print(activeNode==this?(tunning?'>':':'):' ');
			p.print(value);
			p.print(units);
		}
		void clamp() {
      if (value<low) value=low;
      else if (value>high) value=high;
		}
		//lazy drawing, we have no drawing position here... so we will ask the menu to redraw
		virtual promptFeedback activate(menuOut& p,Stream&c,bool canExit=false) {
			if (activeNode!=this) {
				if (action(*this,p,c)) return true;;
			  ox=activeNode->ox;
			  oy=activeNode->oy;
				previousMenu=(menu*)activeNode;
				activeNode=this;
      	p.lastSel=-1;
      	previousMenu->printMenu(p,previousMenu->canExit);
			}
			//printing here to solve U8GLX blank screen on field edit
			// TODO: check Serial out...
			previousMenu->printMenu(p,false);//previousMenu->canExit);
			if (!c.available()) return 0;
			if (strchr(numericChars,c.peek())) {//a numeric value was entered
      	value=c.parseFloat();
    		tunning=false;
    		activeNode=previousMenu;
    		c.flush();
    		ch=menu::enterCode;
      } else {
			  ch=c.read();
		    tmp=value;
		    if (ch==menu::enterCode) {
		    	if (tunning||!tune) {//then exit edition
		    		tunning=false;
		    		activeNode=previousMenu;
		    		c.flush();
		    	} else tunning=true;
		    } else if (ch=='+') value+=tunning?tune:step;
		    else if (ch=='-') value-=tunning?tune:step;
		  }
      clamp();
      if (value!=tmp||ch==menu::enterCode) {
      	func();//call update functions
      	p.lastSel=-1;
      	previousMenu->printMenu(p,previousMenu->canExit);
      }
			return 0;
		}
	};

	template<typename T>
	class menuVariant:public menu {
		public:
		T& target;
		menuVariant(T& target,const char *text,unsigned int sz,menuValue<T>* const data[]):
	    menu(text,sz,(prompt**)data),target(target) {sync();}
		virtual bool needRedraw() {return ((menuValue<T>*)pgm_read_ptr_near(&data[sel]))->value!=target;}
		void sync() {//if possible make selection match the target value
			sel=0;
	  	for(int n=0;n<sz;n++)
	  		if (((menuValue<T>*)pgm_read_ptr_near(&data[n]))->value==target)
	  			sel=n;
    }
		virtual void printTo(menuOut& p) {
			menuVariant<T>::sync();
			print_P(p,text);
			((prompt*)pgm_read_ptr_near(&data[sel]))->printTo(p);
			//print_P(p,((menuValue<T>*)pgm_read_ptr_near(&data[sel]))->text);
		}
  };

	template<typename T>
	class menuSelect: public menuVariant<T> {
		public:
	};

	template<typename T>
	class menuChoice: public menuVariant<T> {
		public:
		menuChoice(const char *text,unsigned int sz,menuValue<T>* const data[],T& target):
	    menuVariant<T>(target,text,sz,data) {menuVariant<T>::sync();}

		//ignore canExit (this exists by select), however we could use a cancel option instead of Exit
		promptFeedback activate(menuOut& p,Stream& c,bool) {
			if (menu::activeNode!=this) {
				if (menuVariant<T>::action(*this,p,c)) return true;
			  this->setPosition(menuNode::activeNode->ox,menuNode::activeNode->oy);
				this->menu::previousMenu=(menu*)menu::activeNode;
				menu::activeNode=this;
			 	this->canExit=false;
				if (p.top>menu::sel) p.top=menu::sel;
				else if (menu::sel+1>p.maxY) p.top=menu::sel-p.maxY+1;
			}
			int op=-1;
			menu::printMenu(p,false);
			op=menu::menuKeys(p,c,false);
			if (op>=0&&op<this->menu::sz) {
				this->menu::sel=op;
				menuValue<T>* cp=(menuValue<T>*)pgm_read_ptr_near(&this->menu::data[op]);
				if (cp->enabled) {
					this->menuVariant<T>::target=cp->value;
					cp->activate(p,c,true);
					//and exit
					this->menu::activeNode=this->menu::previousMenu;
				 	c.flush();//reset the encoder
				}
			}
			return 0;
		}
	};
	template<typename T>
	class menuToggle: public menuVariant<T> {
		public:

		menuToggle(const char *text,unsigned int sz,menuValue<T>* const data[],T& target):
	    menuVariant<T>(target,text,sz,data) {menuVariant<T>::sync();}

		promptFeedback activate(menuOut& p,Stream& c,bool canExit) {
			if (menuVariant<T>::action(*this,p,c)) return true;
			this->menu::sel++;
			if (this->menu::sel>=this->menu::sz) this->menu::sel=0;
		 	p.lastSel=-1;//redraw only affected option
			menuValue<T>* cp=(menuValue<T>*)pgm_read_ptr_near(&this->menu::data[menu::sel]);
			this->menuVariant<T>::target=cp->value;
			cp->activate(p,c,true);
			return 0;
		}
	};
#endif
