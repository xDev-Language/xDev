/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev WebGUI
 * File......: webgui.js
 *
 */

// The runtime of the page, put into every page by the library: the page draws the controls the program sends and
// tells the program what the user did; the logic stays in the program; the library calls this function with the
// text of the style sheet web/webgui.css

"use strict";

( style =>
{
   const aligns = [ "left", "right", "center" ];

   const program = window.chrome.webview;
   const controls = new Map();

   // The shortcuts of the items of the menus: { keys, id, number }
   const shortcuts = [];

   // The client area of the controls and the menu bar above it, made at the first control and the first menu
   let client = null;
   let menubar = null;

   // The open menu, null when all menus are closed
   let openMenu = null;

   // Tell the program an event of a control: wg:<id>:<event>:<value>
   function tell( id, event, value )
   {
      program.postMessage( `wg:${id}:${event}:${value ?? ""}` );
   }

   // The area of the controls under the menu bar, a column
   function clientArea()
   {
      if( !client )
      {
         client = document.createElement( "main" );
         client.className = "wg-client wg-box";
         document.body.append( client );
      }

      return client;
   }

   // The menu bar at the top of the window
   function menuBar()
   {
      if( !menubar )
      {
         menubar = document.createElement( "nav" );
         menubar.className = "wg-menubar";
         document.body.prepend( menubar );
         document.body.classList.add( "wg-has-menu" );
      }

      return menubar;
   }

   // A key as a shortcut is written, e.g. CTRL+SHIFT+N, F2, DEL
   function keyName( ctrl, alt, shift, key )
   {
      const names = { DELETE: "DEL", INSERT: "INS", ESCAPE: "ESC", PAGEUP: "PGUP", PAGEDOWN: "PGDN", " ": "SPACE" };
      const upper = key.toUpperCase();

      return ( ctrl ? "CTRL+" : "" ) + ( alt ? "ALT+" : "" ) + ( shift ? "SHIFT+" : "" ) + ( names[ upper ] ?? upper );
   }

   // A shortcut of the program in the form of keyName(), e.g. "Shift+Ctrl+n" as CTRL+SHIFT+N
   function shortcutName( text )
   {
      const parts = text.toUpperCase().split( "+" );
      const key = parts.pop();

      return keyName( parts.includes( "CTRL" ), parts.includes( "ALT" ), parts.includes( "SHIFT" ), key );
   }

   // The picture of a get; the program sends and gets the value as text: C text, N a number, D YYYYMMDD or empty,
   // L true or false
   class Picture
   {
      constructor( type, picture )
      {
         this.type = type;
         this.upper = picture.startsWith( "@!" );
         this.picture = this.upper ? picture.slice( 2 ).trim() : picture;
         if( type === "N" && this.picture )
            this.number = Picture.numberPicture( this.picture );
         else if( type === "D" )
         {
            this.picture = this.picture || "DD.MM.YYYY";
            this.tokens = this.picture.match( /YYYY|DD|MM/g );
            this.input = this.picture.replace( /YYYY|DD|MM/g, token => "9".repeat( token.length ) );
         }
         else if( type === "L" )
         {
            const parts = ( this.picture || ".T./.F." ).split( "/" );

            this.yes = parts[ 0 ];
            this.no = parts[ 1 ] ?? "";
         }
      }

      // The test of a character for a place of a picture of text: 9 a digit, A and ! a letter, X any character
      static accepts( token )
      {
         const tests = { "9": /[0-9]/, A: /\p{L}/u, "!": /\p{L}/u, X: /[^\x00-\x1f]/ };

         return tests[ token ] ?? null;
      }

      // The parts of a picture of a number: text before and after, digits before and after the decimal separator,
      // the decimal and the group separator
      static numberPicture( picture )
      {
         const first = picture.search( /[9#]/ );
         const last = Math.max( picture.lastIndexOf( "9" ), picture.lastIndexOf( "#" ) );
         const body = picture.slice( first, last + 1 );
         const point = Math.max( body.lastIndexOf( "." ), body.lastIndexOf( "," ) );
         const after = point >= 0 ? body.slice( point + 1 ).replace( /[^9#]/g, "" ).length : 0;
         const both = body.includes( "." ) && body.includes( "," );
         const separator = point >= 0 && ( both || after === 1 || after === 2 ) ? body[ point ] : "";
         const integer = separator ? body.slice( 0, body.lastIndexOf( separator ) ) : body;

         return {
            prefix: picture.slice( 0, first ),
            suffix: picture.slice( last + 1 ),
            integerDigits: integer.replace( /[^9#]/g, "" ).length,
            decimalDigits: separator ? after : 0,
            decimalSeparator: separator,
            groupSeparator: [ ...integer ].find( character => character === "." || character === "," ) ?? ""
         };
      }

      // The longest text the field takes, null without a limit
      maxLength()
      {
         if( this.type === "C" && this.picture )
            return this.picture.replace( /\\(.)/g, "$1" ).length;

         return this.type === "D" ? this.input.length : null;
      }

      // The text of a value of the program
      format( value )
      {
         if( this.type === "N" )
            return this.formatNumber( value );

         if( this.type === "L" )
            return value ? this.yes : this.no;

         if( this.type === "D" )
         {
            const parts = { YYYY: value.slice( 0, 4 ), MM: value.slice( 4, 6 ), DD: value.slice( 6, 8 ) };

            return value ? this.picture.replace( /YYYY|DD|MM/g, token => parts[ token ] ) : "";
         }

         return this.picture ? this.formatText( value, this.picture ) : this.upperCase( value );
      }

      // The text while the user writes: only the characters the picture takes, with its other characters
      formatInput( text )
      {
         if( this.type === "N" )
            return this.formatNumberInput( text );

         if( this.type === "L" )
         {
            const last = text.slice( -1 ).toUpperCase();

            if( last !== "" && "TY1".includes( last ) )
               return this.yes;

            if( last !== "" && "FN0".includes( last ) )
               return this.no;

            return text;
         }

         if( this.type === "D" )
            return this.formatText( text, this.input );

         return this.picture ? this.formatText( text, this.picture ) : this.upperCase( text );
      }

      // The value of a text for the program, null when the text is not a value
      parse( text )
      {
         if( this.type === "N" )
            return this.parseNumber( text );

         if( this.type === "L" )
         {
            const value = text.trim().toUpperCase();

            return value === this.yes.toUpperCase() ? true : value === this.no.toUpperCase() ? false : null;
         }

         if( this.type === "D" )
            return this.parseDate( text );

         return text;
      }

      // Whether the text is a whole value: a picture of text is filled, a number has room in the picture
      isValid( text )
      {
         const value = this.parse( text );

         if( value === null )
            return false;

         if( this.type === "C" && this.picture && text !== "" )
            return this.formatText( text, this.picture ).length === this.maxLength();

         if( this.type === "N" && this.number )
         {
            const integer = Math.abs( value ).toFixed( this.number.decimalDigits ).split( "." )[ 0 ];

            return integer.length <= this.number.integerDigits;
         }

         return true;
      }

      // All letters in capitals for "@!"
      upperCase( text )
      {
         return this.upper ? text.toLocaleUpperCase() : text;
      }

      // Text put into a picture: every place takes the next character it accepts, the other characters of the
      // picture stand between them; "\" makes the next character of the picture an ordinary one
      formatText( value, picture )
      {
         const input = String( value );
         let result = "";
         let pending = "";
         let index = 0;
         let escaped = false;

         for( const token of picture )
         {
            const test = escaped ? null : Picture.accepts( token );

            if( !escaped && token === "\\" )
            {
               escaped = true;
               continue;
            }

            escaped = false;
            if( !test )
            {
               if( result === "" )
                  pending += token;
               else
                  result += token;

               if( input[ index ] === token )
                  ++index;

               continue;
            }

            while( index < input.length && !test.test( input[ index ] ) )
               ++index;

            if( index >= input.length )
               break;

            result += pending + ( token === "!" || this.upper ? input[ index ].toLocaleUpperCase() : input[ index ] );
            pending = "";
            ++index;
         }

         return result;
      }

      // A date as YYYYMMDD from its text, "" for an empty text, null for a wrong date
      parseDate( text )
      {
         const digits = text.replace( /\D/g, "" );
         const parts = {};
         let offset = 0;

         if( digits === "" )
            return "";

         if( digits.length !== 8 )
            return null;

         for( const token of this.tokens )
         {
            parts[ token ] = digits.slice( offset, offset + token.length );
            offset += token.length;
         }

         const date = new Date( Number( parts.YYYY ), Number( parts.MM ) - 1, Number( parts.DD ) );

         if( date.getMonth() !== Number( parts.MM ) - 1 || date.getDate() !== Number( parts.DD ) )
            return null;

         return parts.YYYY + parts.MM + parts.DD;
      }

      // The text of a number by the picture; a number too big for it as it is
      formatNumber( value )
      {
         const number = this.number;

         if( !number )
            return String( value );

         const parts = Math.abs( value ).toFixed( number.decimalDigits ).split( "." );
         let integer = parts[ 0 ];

         if( integer.length > number.integerDigits )
            return String( value );

         if( number.groupSeparator )
            integer = integer.replace( /\B(?=(\d{3})+(?!\d))/g, number.groupSeparator );

         return ( value < 0 ? "-" : "" ) + number.prefix + integer +
                ( number.decimalDigits ? number.decimalSeparator + parts[ 1 ] : "" ) + number.suffix;
      }

      // A number while the user writes: digits of the picture, its separators, a minus anywhere
      formatNumberInput( text )
      {
         const number = this.number;
         const negative = text.includes( "-" );

         if( !number )
         {
            const point = text.search( /[.,]/ );
            const integer = ( point >= 0 ? text.slice( 0, point ) : text ).replace( /\D/g, "" );
            const decimals = point >= 0 ? "." + text.slice( point + 1 ).replace( /\D/g, "" ) : "";

            return ( negative ? "-" : "" ) + integer + decimals;
         }

         let body = text.trim();

         if( number.prefix && body.startsWith( number.prefix ) )
            body = body.slice( number.prefix.length );

         if( number.suffix && body.endsWith( number.suffix ) )
            body = body.slice( 0, -number.suffix.length );

         if( number.groupSeparator )
            body = body.split( number.groupSeparator ).join( "" );

         const point = number.decimalSeparator ? body.indexOf( number.decimalSeparator ) : -1;
         const whole = point >= 0 ? body.slice( 0, point ) : body;
         const fraction = point >= 0 ? body.slice( point + 1 ) : "";
         let integer = whole.replace( /\D/g, "" ).slice( 0, number.integerDigits );
         const decimals = fraction.replace( /\D/g, "" ).slice( 0, number.decimalDigits );

         if( integer === "" && decimals === "" )
            return negative ? "-" : "";

         if( number.groupSeparator )
            integer = integer.replace( /\B(?=(\d{3})+(?!\d))/g, number.groupSeparator );

         return ( negative ? "-" : "" ) + number.prefix + integer +
                ( point >= 0 && number.decimalDigits ? number.decimalSeparator + decimals : "" ) + number.suffix;
      }

      // A number from its text, 0 for an empty text, null for a wrong text
      parseNumber( text )
      {
         let body = text.trim();

         if( body === "" )
            return 0;

         if( this.number )
         {
            body = body.replace( this.number.prefix, "" ).replace( this.number.suffix, "" );
            if( this.number.groupSeparator )
               body = body.split( this.number.groupSeparator ).join( "" );

            if( this.number.decimalSeparator )
               body = body.replace( this.number.decimalSeparator, "." );
         }
         else
            body = body.replace( ",", "." );

         const value = Number( body.replace( /[^0-9.-]/g, "" ) );

         return body === "-" || !Number.isFinite( value ) ? null : value;
      }
   }

   // The pictures of the gets by their input elements, and the last value each get told the program
   const pictures = new Map();
   const told = new Map();

   // The value of a get as the program gets it in an event
   function eventValue( type, value )
   {
      return type === "L" ? ( value ? "T" : "F" ) : String( value );
   }

   // Keep the text of a get as its value: a whole value is shown by the picture and goes to the program when it
   // changed; a wrong text is marked; returns whether the value was kept
   function commitGet( id, input )
   {
      const picture = pictures.get( input );
      const value = picture.parse( input.value );
      const field = input.parentElement;

      if( !picture.isValid( input.value ) )
      {
         field.classList.add( "invalid" );
         return false;
      }

      field.classList.remove( "invalid" );
      input.value = picture.format( value );
      if( told.get( input ) !== eventValue( picture.type, value ) )
      {
         told.set( input, eventValue( picture.type, value ) );
         tell( id, "change", told.get( input ) );
      }

      return true;
   }

   // Give the focus to the next control of the page that takes it
   function focusNext( element )
   {
      const all = Array.from( document.body.querySelectorAll( "input, select, button, .wg-browse" ) );
      const usable = all.filter( control => !control.disabled && !control.closest( "[hidden]" ) );

      usable[ usable.indexOf( element ) + 1 ]?.focus();
   }

   // A field: its caption, the label given by the program, above its input element
   function field( input )
   {
      const element = document.createElement( "div" );
      const caption = document.createElement( "span" );

      caption.className = "wg-caption";
      input.className = "wg-input";
      element.append( caption, input );
      return element;
   }

   /* ---- Menus ---- */

   // The items of a menu that can be chosen
   function menuItems( menu )
   {
      return Array.from( menu.querySelectorAll( ".wg-menu-item:not(:disabled)" ) );
   }

   // Open a menu with items, closing the open one; with focusItem the first item gets the focus
   function openMenuOf( menu, focusItem )
   {
      if( openMenu && openMenu !== menu )
         closeMenu();

      menu.classList.add( "open" );
      openMenu = menu;
      if( focusItem )
         menuItems( menu )[ 0 ]?.focus();
   }

   // Close the open menu; with focusTitle its title gets the focus
   function closeMenu( focusTitle )
   {
      if( !openMenu )
         return;

      openMenu.classList.remove( "open" );
      if( focusTitle )
         openMenu.firstChild.focus();

      openMenu = null;
   }

   // Go to the menu at offset from this one: a menu with items opens, a command of the bar gets the focus
   function switchMenu( menu, offset, focusItem )
   {
      const menus = Array.from( menubar.children );
      const next = menus[ ( menus.indexOf( menu ) + offset + menus.length ) % menus.length ];

      if( next.lastChild.childElementCount === 0 )
      {
         closeMenu();
         next.firstChild.focus();
         return;
      }

      openMenuOf( next, focusItem );
      if( !focusItem )
         next.firstChild.focus();
   }

   // The keys of the title of a menu: Down opens it, Left and Right go to the next menu, Esc closes it
   function menuTitleKey( menu, event )
   {
      const moves = { ArrowLeft: -1, ArrowRight: 1 };

      if( event.key === "ArrowDown" && menu.lastChild.childElementCount )
         openMenuOf( menu, true );
      else if( event.key in moves )
         switchMenu( menu, moves[ event.key ], false );
      else if( event.key === "Escape" )
         closeMenu();
      else
         return;

      event.preventDefault();
   }

   // The keys of the items of a menu: Up, Down, Home and End move, Left and Right go to the next menu, Esc closes
   function menuItemKey( menu, event )
   {
      const items = menuItems( menu );
      const index = items.indexOf( document.activeElement );
      const moves = { ArrowDown: index + 1, ArrowUp: index - 1 + items.length, Home: 0, End: items.length - 1 };

      if( items.length === 0 )
         return;

      if( event.key in moves )
         items[ moves[ event.key ] % items.length ].focus();
      else if( event.key === "ArrowLeft" || event.key === "ArrowRight" )
         switchMenu( menu, event.key === "ArrowLeft" ? -1 : 1, true );
      else if( event.key === "Escape" )
         closeMenu( true );
      else
      {
         if( event.key === "Tab" )
            closeMenu();

         return;
      }

      event.preventDefault();
   }

   // Add an item to a menu, a separator without text; its number counts the separators too, as the program does
   function addItem( control, command )
   {
      const items = control.element.lastChild;
      const number = items.childElementCount + 1;

      if( command.text == null )
      {
         const line = document.createElement( "div" );

         line.className = "wg-menu-separator";
         items.append( line );
         return;
      }

      const item = document.createElement( "button" );
      const label = document.createElement( "span" );

      item.type = "button";
      item.className = "wg-menu-item";
      item.tabIndex = -1;
      label.textContent = command.text;
      item.append( label );
      if( command.shortcut )
      {
         const keys = document.createElement( "span" );

         keys.className = "wg-menu-shortcut";
         keys.textContent = command.shortcut;
         item.append( keys );
         shortcuts.push( { keys: shortcutName( command.shortcut ), id: command.id, number: number } );
      }

      item.onclick = () =>
      {
         closeMenu();
         tell( command.id, "item", number );
      };
      items.append( item );
   }

   /* ---- Browses ---- */

   // The rows of a browse
   function rowsOf( element )
   {
      return element.querySelector( "tbody" ).rows;
   }

   // The number of the current row of a browse, 0 for none
   function currentRow( element )
   {
      const row = element.querySelector( "tbody tr.current" );

      return row ? row.sectionRowIndex + 1 : 0;
   }

   // Mark a row of a browse as the current one and scroll it into view; 0 marks none
   function markRow( element, number )
   {
      const rows = rowsOf( element );

      element.querySelector( "tbody tr.current" )?.classList.remove( "current" );
      if( number >= 1 && number <= rows.length )
      {
         rows[ number - 1 ].classList.add( "current" );
         rows[ number - 1 ].scrollIntoView( { block: "nearest" } );
      }
   }

   // The user moves to another row of a browse; the program learns its number
   function moveRow( id, element, number )
   {
      const count = rowsOf( element ).length;
      const target = Math.max( 1, Math.min( number, count ) );

      if( count === 0 || target === currentRow( element ) )
         return;

      markRow( element, target );
      tell( id, "change", target );
   }

   // The keys of a browse: arrows, pages, Home and End move the current row, Enter chooses it
   function browseKey( id, element, event )
   {
      const rows = rowsOf( element );
      const row = currentRow( element );
      const height = rows.length ? rows[ 0 ].offsetHeight : 1;
      const page = Math.max( 1, Math.floor( element.clientHeight / height ) - 1 );
      const moves = { ArrowDown: row + 1, ArrowUp: row - 1, PageDown: row + page, PageUp: row - page,
                      Home: 1, End: rows.length };

      if( event.key === "Enter" )
         tell( id, "click" );
      else if( event.key in moves )
         moveRow( id, element, moves[ event.key ] );
      else
         return;

      event.preventDefault();
   }

   // The headings of a browse
   function browseColumns( control, columns )
   {
      const heading = document.createElement( "tr" );

      control.columns = columns;
      for( const column of columns )
      {
         const cell = document.createElement( "th" );

         cell.textContent = column.heading;
         cell.style.textAlign = aligns[ column.align ];
         if( column.width != null )
            cell.style.width = column.width + "px";

         heading.append( cell );
      }

      control.element.querySelector( "thead" ).replaceChildren( heading );
   }

   // The rows of a browse, the texts of their cells
   function browseRows( control, rows )
   {
      const body = control.element.querySelector( "tbody" );

      body.replaceChildren();
      for( const texts of rows )
      {
         const row = body.insertRow();

         texts.forEach( ( text, index ) =>
         {
            const cell = row.insertCell();

            cell.textContent = text;
            cell.style.textAlign = aligns[ control.columns[ index ].align ];
         } );
      }
   }

   /* ---- Sidebars and splits ---- */

   // The client area stands at the right of the sidebar
   function layoutClient()
   {
      const sidebar = Array.from( controls.values() ).find( control => control.kind === "sidebar" );

      clientArea().style.left = sidebar ? sidebar.element.offsetWidth + "px" : "";
   }

   // A button with a picture of the program, a data URI, and its tip
   function pictureButton( className, picture, tip )
   {
      const button = document.createElement( "button" );
      const image = document.createElement( "img" );

      button.type = "button";
      button.className = className;
      button.title = tip ?? "";
      image.src = picture;
      button.append( image );
      return button;
   }

   // Show page number of a sidebar, 0 hides its panel: the picture of the page is marked, the header shows its title
   // and its actions; the client area follows the new width
   function showPage( side, number )
   {
      const panel = side.element.querySelector( ".wg-side-panel" );

      side.page = number;
      side.pages.forEach( ( page, index ) =>
      {
         page.button.classList.toggle( "active", index + 1 === number );
         page.actions.hidden = index + 1 !== number;
         page.element.hidden = index + 1 !== number;
      } );
      panel.hidden = number < 1 || number > side.pages.length;
      panel.style.width = side.width + "px";
      side.element.querySelector( ".wg-side-title" ).textContent = panel.hidden ? "" : side.pages[ number - 1 ].title;
      layoutClient();
   }

   // A page of a sidebar for a new panel: its picture in the strip, its actions in the header, the panel under it;
   // a click on the picture shows the page or hides the shown one and tells the program
   function addPage( side, panel, command )
   {
      const number = side.pages.length + 1;
      const button = pictureButton( "wg-strip-button", command.items[ 0 ], command.items[ 1 ] );
      const actions = document.createElement( "div" );

      actions.className = "wg-side-actions";
      button.onclick = () =>
      {
         const shown = side.page === number ? 0 : number;

         showPage( side, shown );
         tell( side.id, "change", shown );
      };
      side.element.querySelector( ".wg-strip" ).firstChild.append( button );
      side.element.querySelector( ".wg-side-header" ).append( actions );
      side.element.querySelector( ".wg-side-pages" ).append( panel.element );
      side.pages.push( { title: command.text, button: button, actions: actions, element: panel.element } );
      showPage( side, side.page );
   }

   // The mouse drags the right edge of the panel of a sidebar: the panel takes the new width, from 170 pixels up to
   // the width of the window less 150
   function dragEdge( side, event )
   {
      const edge = event.target;
      const start = event.clientX;
      const width = side.width;

      edge.setPointerCapture( event.pointerId );
      edge.onpointermove = move =>
      {
         side.width = Math.max( 170, Math.min( width + move.clientX - start, window.innerWidth - 150 ) );
         showPage( side, side.page );
      };
      edge.onpointerup = () => edge.onpointermove = null;
   }

   // The mouse drags the line of a split: the left part takes the new width, leaving 120 pixels to each part; the
   // program learns the width when the mouse lets the line go
   function dragDivider( split, event )
   {
      const divider = event.target;
      const start = event.clientX;
      const width = split.position;

      divider.setPointerCapture( event.pointerId );
      divider.onpointermove = move =>
      {
         const most = split.element.clientWidth - divider.offsetWidth - 120;

         split.position = Math.max( 120, Math.min( width + move.clientX - start, most ) );
         split.first.style.width = split.position + "px";
      };
      divider.onpointerup = () =>
      {
         divider.onpointermove = null;
         tell( split.id, "change", Math.round( split.position ) );
      };
   }

   // The elements of the kinds of controls; every kind tells its own events
   const kinds =
   {
      label()
      {
         return document.createElement( "span" );
      },

      button( command )
      {
         const element = document.createElement( "button" );

         element.type = "button";
         element.onclick = () => tell( command.id, "click" );
         return element;
      },

      edit( command )
      {
         const input = document.createElement( "input" );

         input.type = "text";
         input.oninput = () => tell( command.id, "change", input.value );
         return field( input );
      },

      check( command )
      {
         const element = document.createElement( "label" );
         const box = document.createElement( "input" );

         box.type = "checkbox";
         box.onchange = () => tell( command.id, "change", box.checked ? "T" : "F" );
         element.append( box, document.createElement( "span" ) );
         return element;
      },

      combo( command )
      {
         const input = document.createElement( "select" );

         for( const item of command.items ?? [] )
            input.add( new Option( item ) );

         input.onchange = () => tell( command.id, "change", input.selectedIndex + 1 );
         return field( input );
      },

      get( command )
      {
         const input = document.createElement( "input" );
         const picture = new Picture( command.type, command.picture ?? "" );
         const element = field( input );

         pictures.set( input, picture );
         input.type = "text";
         if( picture.maxLength() !== null )
            input.maxLength = picture.maxLength();

         if( command.type === "N" )
            element.classList.add( "number" );

         input.oninput = () =>
         {
            input.value = picture.formatInput( input.value );
            element.classList.remove( "invalid" );
         };
         input.onfocus = () => tell( command.id, "focus" );
         input.onblur = () => commitGet( command.id, input );
         input.onkeydown = event =>
         {
            if( event.key !== "Enter" )
               return;

            event.preventDefault();
            if( commitGet( command.id, input ) )
               focusNext( input );
         };
         return element;
      },

      browse( command )
      {
         const element = document.createElement( "div" );
         const table = document.createElement( "table" );

         table.append( document.createElement( "thead" ), document.createElement( "tbody" ) );
         element.append( table );
         element.tabIndex = 0;
         element.onclick = event =>
         {
            const row = event.target.closest( "tbody tr" );

            if( row )
               moveRow( command.id, element, row.sectionRowIndex + 1 );
         };
         element.ondblclick = event =>
         {
            if( event.target.closest( "tbody tr" ) )
               tell( command.id, "click" );
         };
         element.onkeydown = event => browseKey( command.id, element, event );
         return element;
      },

      menu( command )
      {
         const element = document.createElement( "div" );
         const title = document.createElement( "button" );
         const items = document.createElement( "div" );

         title.type = "button";
         title.className = "wg-menu-title";
         items.className = "wg-menu-items";
         element.append( title, items );
         title.onclick = () =>
         {
            if( items.childElementCount === 0 )
            {
               closeMenu();
               tell( command.id, "click" );
            }
            else if( openMenu === element )
               closeMenu();
            else
               openMenuOf( element, false );
         };
         title.onmouseenter = () =>
         {
            if( openMenu && openMenu !== element && items.childElementCount )
               openMenuOf( element, false );
         };
         title.onkeydown = event => menuTitleKey( element, event );
         items.onkeydown = event => menuItemKey( element, event );
         return element;
      },

      sidebar()
      {
         const element = document.createElement( "div" );
         const strip = document.createElement( "div" );
         const panel = document.createElement( "div" );
         const header = document.createElement( "div" );
         const title = document.createElement( "span" );
         const pages = document.createElement( "div" );
         const edge = document.createElement( "div" );

         strip.className = "wg-strip";
         strip.append( document.createElement( "div" ), document.createElement( "div" ) );
         title.className = "wg-side-title";
         header.className = "wg-side-header";
         header.append( title );
         pages.className = "wg-side-pages";
         edge.className = "wg-side-edge";
         panel.className = "wg-side-panel";
         panel.append( header, pages, edge );
         element.append( strip, panel );
         return element;
      },

      panel()
      {
         return document.createElement( "div" );
      },

      column()
      {
         return document.createElement( "div" );
      },

      row()
      {
         return document.createElement( "div" );
      },

      split()
      {
         const element = document.createElement( "div" );
         const divider = document.createElement( "div" );

         divider.className = "wg-divider";
         element.append( divider );
         return element;
      }
   };

   // Set the properties of a command on a control: place, size, growth, text, value, enabled, visible; null takes a
   // place or a size away; a control with x or y stands out of the order of its parent; the text of a field is its
   // label; a sidebar takes the width of its panel and the shown page, a split the width of its left part
   function apply( control, command )
   {
      const element = control.element;
      const input = control.input ?? element;
      const places = { x: "left", y: "top", width: "width", height: "height" };

      if( control.kind === "sidebar" )
      {
         control.width = command.width ?? control.width;
         showPage( control, command.value ?? control.page );
         return;
      }

      for( const [ name, property ] of Object.entries( places ) )
      {
         if( name in command )
            element.style[ property ] = command[ name ] === null ? "" : command[ name ] + "px";
      }

      element.classList.toggle( "wg-placed", element.style.left !== "" || element.style.top !== "" );
      if( "grow" in command )
         element.classList.toggle( "wg-grow", command.grow === true );

      if( command.text != null )
      {
         if( control.kind === "check" )
            element.lastChild.textContent = command.text;
         else if( control.input || control.kind === "menu" )
            element.firstChild.textContent = command.text;
         else
            element.textContent = command.text;
      }

      if( command.columns != null )
         browseColumns( control, command.columns );

      if( command.rows != null )
         browseRows( control, command.rows );

      if( command.value != null )
      {
         if( control.kind === "split" )
         {
            control.position = command.value;
            if( control.first )
               control.first.style.width = command.value + "px";
         }
         else if( control.kind === "check" )
            element.firstChild.checked = command.value;
         else if( control.kind === "combo" )
            input.selectedIndex = command.value - 1;
         else if( control.kind === "browse" )
            markRow( element, command.value );
         else if( control.kind === "get" )
         {
            input.value = pictures.get( input ).format( command.value );
            element.classList.remove( "invalid" );
            told.set( input, eventValue( pictures.get( input ).type, command.value ) );
         }
         else
            input.value = command.value;
      }

      if( command.enabled != null )
      {
         if( control.kind === "check" )
         {
            element.firstChild.disabled = !command.enabled;
            element.classList.toggle( "disabled", !command.enabled );
         }
         else if( control.kind === "menu" )
            element.firstChild.disabled = !command.enabled;
         else
            input.disabled = !command.enabled;
      }

      if( command.visible != null )
         element.hidden = !command.visible;
   }

   // Put a new control into its parent: a box, a part of a split, the left one before the line, or the client area
   function place( command, element )
   {
      const parent = command.parent ? controls.get( command.parent ) : null;

      if( parent && parent.kind === "split" && !parent.first )
      {
         parent.first = element;
         element.style.flex = "0 0 auto";
         element.style.width = parent.position + "px";
         parent.element.prepend( element );
         return;
      }

      ( parent ? parent.element : clientArea() ).append( element );
   }

   // Create a control: a menu goes to the menu bar, a sidebar to the left edge, a panel to its sidebar, any other
   // control to its parent or to the client area
   function create( command )
   {
      const element = kinds[ command.kind ]( command );
      const created = { kind: command.kind, element: element, id: command.id };
      const classes = { menu: "wg-menu", sidebar: "wg-sidebar", panel: "wg-side-page wg-box" };
      const fields = { edit: "wg-field", combo: "wg-field", get: "wg-field" };

      const own = [ "wg-control", "wg-" + command.kind, fields[ command.kind ] ?? "" ].join( " " ).trim();

      element.className = classes[ command.kind ] ?? own;
      if( fields[ command.kind ] )
         created.input = element.lastChild;

      controls.set( command.id, created );
      switch( command.kind )
      {
      case "menu":
         apply( created, command );
         menuBar().append( element );
         break;

      case "sidebar":
         Object.assign( created, { pages: [], page: 0, width: 260 } );
         element.querySelector( ".wg-side-edge" ).onpointerdown = event => dragEdge( created, event );
         document.body.append( element );
         apply( created, command );
         break;

      // The value of a panel is the number of its sidebar
      case "panel":
         addPage( controls.get( command.value ), created, command );
         break;

      case "split":
         Object.assign( created, { first: null, position: 300 } );
         element.firstChild.onpointerdown = event => dragDivider( created, event );
         apply( created, command );
         place( command, element );
         break;

      default:
         apply( created, command );
         place( command, element );
         break;
      }
   }

   // Run a command of the program: create, set, item, action, invalid, skip, focus, destroy
   function run( command )
   {
      const control = controls.get( command.id );

      switch( command.cmd )
      {
      case "create":
         create( command );
         break;

      case "set":
         apply( control, command );
         break;

      case "item":
         addItem( control, command );
         break;

      // An action in the header of a page of a sidebar, or a button at the bottom of its strip for page 0
      case "action":
      {
         const page = command.page ? control.pages[ command.page - 1 ] : null;
         const button = pictureButton( page ? "wg-side-action" : "wg-strip-button", command.picture, command.tip );

         button.onclick = () => tell( command.id, "item", command.number );
         ( page ? page.actions : control.element.querySelector( ".wg-strip" ).lastChild ).append( button );
         break;
      }

      // VALID of the program refused the value of a get: the text stays marked, the get gets the focus back and
      // its next value goes to the program again
      case "invalid":
         control.element.classList.add( "invalid" );
         told.delete( control.input );
         control.input.focus();
         break;

      // WHEN of the program does not let the user into a get
      case "skip":
         if( document.activeElement === control.input )
            focusNext( control.input );
         break;

      case "focus":
      {
         const first = [ "check", "menu" ].includes( control.kind ) ? control.element.firstChild : control.element;

         ( control.input ?? first ).focus();
         break;
      }

      case "destroy":
         control.element.remove();
         controls.delete( command.id );
         break;
      }
   }

   program.addEventListener( "message", event =>
   {
      if( event.data && event.data.cmd )
         run( event.data );
   } );

   // The shortcut of an item of a menu runs the item; a key without Ctrl and Alt, other than F1 to F24, stays with
   // a field, so Del there deletes a character; a click outside the open menu closes it
   document.addEventListener( "keydown", event =>
   {
      const name = keyName( event.ctrlKey, event.altKey, event.shiftKey, event.key );
      const found = shortcuts.find( shortcut => shortcut.keys === name && controls.has( shortcut.id ) );
      const typing = [ "INPUT", "SELECT" ].includes( event.target.tagName ) && !event.ctrlKey && !event.altKey;

      if( found && !( typing && !/^F\d+$/.test( event.key ) ) )
      {
         event.preventDefault();
         closeMenu();
         tell( found.id, "item", found.number );
      }
   } );

   document.addEventListener( "mousedown", event =>
   {
      if( openMenu && !openMenu.contains( event.target ) )
         closeMenu();
   } );

   window.addEventListener( "blur", () => closeMenu() );

   document.addEventListener( "DOMContentLoaded", () =>
   {
      const sheet = document.createElement( "style" );

      sheet.textContent = style;
      document.head.prepend( sheet );
      program.postMessage( "wg:ready" );
   } );
} )
