MetalScroll is an alternative for [RockScroll](http://www.hanselman.com/blog/IntroducingRockScroll.aspx), a Visual Studio add-in which replaces the editor scrollbar with a graphic representation of the code. Compared to the original, this version has a number of improvements:

  * double-clicking the scrollbar brings up an options dialog where the color scheme and various other things can be configured.
  * the width of the scrollbar is configurable.
  * the widget at the top of the scrollbar which splits the editor into two panes is still usable when the add-in is active.
  * you must hold down ALT when double-clicking a word to highlight all its occurrences in the file. RockScroll highlights words on regular double-click, which can be annoying when you actually meant to use drag&drop text editing, for example when dragging a variable to the watches window. People who prefer the old behavior can disable this in the options dialog.
  * pressing ESC clears the highlight markers.
  * lines containing highlighted words are marked with 5x5 pixel blocks on the right edge of the scrollbar, to make them easier to find (similar to breakpoints and bookmarks).
  * multiline comments are recognized.
  * hidden text regions and word wrapping are supported.
  * it works in split windows.
  * middle-clicking the scrollbar shows a preview of the code around the clicked line
  * it's open source, so people who want to change stuff or add features can do so themselves.

The add-in works with Visual Studio 2005 and 2008 (Express versions not supported, since they don't allow add-ins). VS 2010 and above will not be supported, but you can use [Progressive Scroll](https://code.google.com/p/progressive-scroll/) for those versions, which has pretty much the same features. There's also an add-in pack from Microsoft called  [Productivity Power Tools](http://visualstudiogallery.msdn.microsoft.com/d0d33361-18e2-46c0-8ff2-4adea1e34fef?SRC=Home) which includes a similar scrollbar replacement.

UPDATE: someone has made a better version for

I can be reached at mihnea.balta@gmail.com for questions, comments, patches, spam etc.

RockScroll (formally called "Microsoft Document Overview Scrollbar") is (c) Microsoft Corporation and was written by Rocky Downs.

### Release Notes ###

Version 1.0.11 (26-Jul-2011)
  * support for UnrealScript by Thaddaeus Frogley
  * "case sensitive" and "whole word" checkboxes in the options dialog by Brook Jones

Version 1.0.10 (15-Sep-2010)
  * fixed excessive spacing and missing bold font for keywords and highlighted text in the code preview window on Windows 7

Version 1.0.9 (14-Sep-2010)
  * whitespace is no longer highlighted on alt-double click
  * the minimum scrollbar width is forced to 8
  * added a command which toggles the visibility of the enhanced scrollbar (MetalScroll.Connect.Toggle)
  * the add-in refuses to load if Rockscroll is present
  * the installer disables Rockscroll if it's found
  * fixed: if the add-in was loaded manually after VS was started, the windows which were already open didn't get a MetalScroll bar

Version 1.0.7 (22-Nov-2009)
  * added support for word wrapping
  * middle click preview improvements:
    * tabs are correctly expanded inside strings and comments
    * multi-line comments are recognized
    * it works with word wrapping and hidden text regions
    * highlighted words are recognized inside strings and comments
  * the current page marker has a minimum height of 15 pixels
  * fixed: in the middle click preview, an escaped backslash right before the terminating quote of a string was parsed as an escaped quote, leading to incorrect highlighting in the following code
  * fixed: // and /`*` inside strings caused the following text to be incorrectly painted as a comment on the bar
  * fixed: placing a breakpoint on the last line in a file could lead to a crash
  * fixed: horizontal dark lines sometimes appeared on the bar when the code image had to be scaled to fit
  * fixed: the add-in didn't work if the horizontal scrollbar of the text editor was turned off in the VS options

Version 1.0.6
  * the scrollbar only colors C-style comments in C-like language files (i.e. C, C++ and C#)
  * the middle click preview only applies syntax highlighting for C-like language files
  * words highlighted by (alt-)double-click are also marked in the middle-click preview window
  * fixed: some words were mistakenly highlighted as C++ keywords in the middle click preview

Version 1.0.5
  * holding down ALT for double-click highlighting is optional
  * when the code image is taller than the scrollbar, it is scaled with a linear filter
  * replaced the middle-click preview tooltip with a custom window which uses a fixed width font and features basic syntax highlighting and proper tab handling
  * fixed: right edge markers (breakpoints, bookmarks etc.) were drawn at a hardcoded location instead of respecting the real bar width
  * fixed: double-click highlighting was also marking partial matches; it only matches whole words now
  * fixed: tabs were always expanded as tab-size pixels, instead of jumping to the next tab-size-aligned column

Version 1.0.4
  * fixed: alt-double click on a blank line freezes the IDE

Version 1.0.2
  * middle-clicking the scrollbar displays a tooltip which contains the code surrounding the clicked line.

Version 1.0.1
  * initial release.

### Complete Feature List ###

For people unfamiliar with the original, here's MetalScroll in all its glory:

![http://metalscroll.googlecode.com/files/MetalScroll.png](http://metalscroll.googlecode.com/files/MetalScroll.png)

  * comments are green, uppercase letters are black, other characters are gray. Change the colors by double-clicking the bar.
  * the blue dots on the right are bookmarks.
  * the red dots are breakpoints.
  * the blue-tinted area is the current page.
  * if you hold down ALT and double-click a word, all its occurrences inside the file are highlighted. In this screenshot, "Advise" was double-clicked.
  * hit ESC or right-click the scrollbar to remove the highlighting.
  * the orange dots are lines where the highlighted word appears. If you squint you can also see that the highlighted word itself is drawn in orange on the scrollbar.
  * the orange markers on the left edge of the scrollbar indicate lines changed since the last save. The blue markers indicate lines changed since the file was opened, but before the last save.
  * middle-clicking the scrollbar displays a window containing the code surrounding the clicked line. In the following screenshot, a line near the second orange dot was middle-clicked, to see how "Advise" is being used there:
![http://metalscroll.googlecode.com/files/CodePreview2.png](http://metalscroll.googlecode.com/files/CodePreview2.png)
  * the colors used for the highlight marker can be changed in the VS Fonts and Colors dialog:
![http://metalscroll.googlecode.com/files/MarkerColor.png](http://metalscroll.googlecode.com/files/MarkerColor.png)
  * double-clicking the scrollbar brings up an options dialog where the appearance of the scrollbar can be customized:
![http://metalscroll.googlecode.com/files/OptionsDialog.png](http://metalscroll.googlecode.com/files/OptionsDialog.png)

If you must know, it's called MetalScroll because classic rock evolved into heavy metal. \m/

### Known Issues ###

  * only C-like languages are supported, so VB, XML, HTML etc. files won't show comments in a special color on the scrollbar and won't have syntax highlighting in the middle-click preview window.
  * the splitter widget found above the scrollbar doesn't know that the scrollbar is wider, so there's a small area to its left which doesn't get painted and displays garbage. I'd need to subclass the editor container window to fix this, but I'm not in the mood right now.