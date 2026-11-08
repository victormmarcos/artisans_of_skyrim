class ArtisansMenu {
    var root:MovieClip;
    var canvas:MovieClip;
    var list:MovieClip;
    var card:MovieClip;
    var modal:MovieClip;
    var data:Object;
    var matches:Array;
    var categories:Array;
    var groups:Array;
    var memory:Object;
    var selected:Number;
    var offset:Number;
    var category:String;
    var group:String;
    var sorting:Number;
    var descending:Boolean;
    var search:String;
    var searching:Boolean;
    var busy:Boolean;
    var confirming:Boolean;
    var confirmationCommand:String;
    var renaming:Boolean;
    var renameValue:String;
    var renameOriginal:String;
    var renameBeforeEdit:String;
    var renameApplied:Boolean;
    var popupOpen:Boolean;
    var popupIndex:Number;
    var popupOptions:Array;
    var sortLabels:Array;
    var sortFields:Array;
    var gamepad:Boolean;
    var playstation:Boolean;
    var tabsTop:Number;
    var searchTop:Number;
    var headerTop:Number;
    var rowTop:Number;
    var rowH:Number = 34;
    var lastNavCommand:String;
    var lastNavAt:Number;
    var pendingMouse:Function;
    var pendingPreview:Object;
    // Wide screens: the list follows the left edge and the details the right
    // edge (like the inventory). Values are in 1280x720 stage units.
    var leftShift:Number = 0;
    var rightShift:Number = 0;
    var viewX:Number = 0;
    var viewY:Number = 0;
    var viewW:Number = 1280;
    var viewH:Number = 720;
    var maxShift:Number = 320;
    var rotating:Boolean = false;
    var wheelLock:Boolean = false;
    var lockX:Number = 0;
    var lockY:Number = 0;

    var warm:Number = 0xB8BCC2;
    var white:Number = 0xF0F1F2;
    var muted:Number = 0xA5A8AD;
    var dim:Number = 0x73777D;
    var danger:Number = 0xD78C7C;
    var picked:Number = 0x91B89A;
    var font:String = "$EverywhereFont";
    var visibleRows:Number = 10;

    function ArtisansMenu(mc:MovieClip) {
        root = mc;
        memory = {};
        matches = [];
        selected = 0; offset = 0; sorting = 0; descending = false;
        category = "All"; group = "All"; search = ""; searching = false;
        popupOpen = false; popupIndex = 0; popupOptions = [];
        confirmationCommand = "activate";
        renaming = false; renameValue = ""; renameOriginal = ""; renameBeforeEdit = ""; renameApplied = false;
        lastNavCommand = ""; lastNavAt = 0;
        gamepad = false; playstation = false;
        _global["gfxExtensions"] = true;
        Stage.scaleMode = "showAll";
        Stage.align = "";
        root._focusrect = false;

        var self:ArtisansMenu = this;
        root.setPage = function(value:Object):Void { self.setPage(value); };
        root.navigate = function(command:String):Void { self.navigate(command); };
        root.setInputMode = function(value:Boolean):Void { self.setInputMode(value); };
        root.onEnterFrame = function():Void {
            var fn:Function=self.pendingMouse;
            self.pendingMouse=undefined;
            if(fn!=undefined) fn();
            // Native model work must not re-enter setPage while it builds the display list.
            var request:Object=self.pendingPreview;
            self.pendingPreview=undefined;
            if(request!=undefined && self.data!=undefined && request.revision==self.data.revision && self.root.aosCommand!=undefined)
                self.root.aosCommand(request.command,request.index,request.revision,"");
        };
        root.SetPlatform = function(platform:Number, psSwitch:Boolean):Void { self.setPlatform(platform,psSwitch); };
        // Global mouse listener: rotation starts on a press over the 3D area
        // whatever is drawn there, and mouse movement ends a wheel scroll.
        var mouse:Object = {};
        mouse.onMouseDown = function():Void { self.mouseDown(); };
        mouse.onMouseUp = function():Void { self.stopRotation(); };
        mouse.onMouseMove = function():Void { self.mouseMoved(); };
        Mouse.addListener(mouse);
    }

    function mouseDown():Void {
        if(canvas==undefined || gamepad || confirming || busy || popupOpen) return;
        var x:Number=canvas._xmouse; var y:Number=canvas._ymouse;
        if(x>=650+rightShift && x<=1244+rightShift && y>=24 && y<=434) { rotating=true; send("rotateStart",0); }
    }

    function mouseMoved():Void {
        if(!wheelLock || canvas==undefined) return;
        if(Math.abs(canvas._xmouse-lockX)+Math.abs(canvas._ymouse-lockY)>6) wheelLock=false;
    }

    // Translated interface text (sent by the plugin); English when missing.
    function T(english:String):String {
        var labels:Object = data == undefined ? undefined : data.labels;
        var value:String = labels == undefined ? undefined : labels[english];
        if (value == undefined || value.length == 0) return english;
        if (value.length > 2 && value == value.toUpperCase() && english != english.toUpperCase()) {
            var words:Array = value.toLowerCase().split(" ");
            for (var i:Number=0;i<words.length;i++) words[i] = words[i].charAt(0).toUpperCase()+words[i].substr(1);
            return words.join(" ");
        }
        return value;
    }

    // Titles (columns, tabs, card labels, headings) are written in capitals.
    function U(english:String):String { return T(english).toUpperCase(); }

    // Values and messages start with a capital letter.
    function cap(v):String {
        var s:String=asText(v);
        if(s.length==0) return s;
        if(s.length>2 && s==s.toUpperCase() && s!=s.toLowerCase()) {
            var words:Array=s.toLowerCase().split(" ");
            for(var i:Number=0;i<words.length;i++) words[i]=words[i].charAt(0).toUpperCase()+words[i].substr(1);
            return words.join(" ");
        }
        return s.charAt(0).toUpperCase()+s.substr(1);
    }

    function capLines(v):String {
        var lines:Array=asText(v).split("\n");
        for(var i:Number=0;i<lines.length;i++) lines[i]=cap(lines[i]);
        return lines.join("\n");
    }

    function finalizeText():String { return asText(data.finalizeLabel).length>0?data.finalizeLabel:T("Craft"); }

    function refreshButtons():Void {
        drawHints();
        updateModalLabels();
        if(card!=undefined && card.action!=undefined && matches[selected]!=undefined)
            styleActionButton(card.action,matches[selected].action);
        if(card!=undefined && card.craft!=undefined) styleCraftButton(card.craft,finalizeText());
    }

    function setInputMode(value:Boolean):Void {
        if(gamepad==value) return;
        gamepad = value;
        refreshButtons();
    }

    function setPlatform(platform:Number, psSwitch:Boolean):Void {
        playstation = psSwitch == true || platform == 2 || platform == 3;
        ArtisansDraw.playstation = playstation;
        if (platform != 0) setInputMode(true);
        refreshButtons();
    }

    function box(parent:MovieClip, name:String, x:Number, y:Number, w:Number, h:Number, color:Number, alpha:Number):MovieClip {
        return ArtisansDraw.box(parent,name,x,y,w,h,color,alpha);
    }

    function line(parent:MovieClip, name:String, x:Number, y:Number, w:Number, alpha:Number):MovieClip {
        return box(parent,name,x,y,w,1,0xFFFFFF,alpha);
    }

    function text(parent:MovieClip, name:String, value:String, x:Number, y:Number, w:Number, h:Number, size:Number, color:Number):TextField {
        return ArtisansDraw.text(parent,name,value,x,y,w,h,size,color,font);
    }

    function align(field:TextField, how:String):Void { ArtisansDraw.align(field,how); }

    function queueMouse(fn:Function):Void {
        if(!gamepad && !busy) pendingMouse=fn;
    }

    // Button like the game's: key icon and label, no box. Hover brightens it.
    function button(parent:MovieClip, name:String, label:String, x:Number, y:Number, w:Number, fn:Function):MovieClip {
        var mc:MovieClip = box(parent,name,x,y,w,34,0x000000,0);
        mc.buttonWidth=w;
        box(mc,"hover",0,0,w,34,warm,0);
        var t:TextField = text(mc,"label",label,8,4,w-16,28,18,white);
        t.wordWrap = false;
        align(t,"center");
        var self:ArtisansMenu=this;
        mc.onRelease=function():Void { self.queueMouse(fn); };
        mc.onRollOver=function():Void { if(!self.gamepad && this.enabled) this.hover._alpha=12; };
        mc.onRollOut=function():Void { this.hover._alpha=0; };
        mc.useHandCursor=false;
        return mc;
    }

    function badgeWidth(key:String):Number { return ArtisansDraw.badgeWidth(key); }

    function controlBadge(parent:MovieClip,name:String,key:String,x:Number,y:Number):Number {
        return ArtisansDraw.badge(parent,name,key,x,y,font,warm,white);
    }

    function drawPrompt(parent:MovieClip,name:String,key:String,label:String,command:String,x:Number,w:Number):Void {
        var badgeW:Number=badgeWidth(key);
        var prompt:TextField=text(parent,name+"Label",label,0,0,w,24,15,muted);
        prompt.wordWrap=false;
        var labelW:Number=Math.min(w-badgeW-8,prompt.textWidth+6);
        var start:Number=x+Math.max(0,(w-badgeW-8-labelW)/2);
        controlBadge(parent,name+"Badge",key,start,0);
        prompt._x=start+badgeW+8; prompt._width=labelW;
        prompt._y=Math.round((23-prompt.textHeight)/2)-2;
        align(prompt,"left");
        if(command.length>0) {
            var hit:MovieClip=box(parent,name+"Hit",x,0,w,27,0,0);
            var self:ArtisansMenu=this;
            hit.useHandCursor=false;
            hit.onRelease=function():Void { self.queueMouse(function():Void { self.navigate(command); }); };
        }
    }

    function styleCommandButton(action:MovieClip,label:String,key:String):Void {
        if(action.keyBadge!=undefined) action.keyBadge.removeMovieClip();
        action.label.text=label;
        var w:Number=action.buttonWidth;
        action.label._x=8; action.label._width=w-16;
        align(action.label,"center");
        if(key.length==0) return;
        var badgeW:Number=badgeWidth(key);
        var labelW:Number=Math.min(w-badgeW-32,action.label.textWidth+8);
        var start:Number=(w-badgeW-8-labelW)/2;
        controlBadge(action,"keyBadge",key,start,5);
        action.label._x=start+badgeW+8; action.label._width=labelW;
        align(action.label,"center");
    }

    function styleActionButton(action:MovieClip,label:String):Void {
        styleCommandButton(action,label,!gamepad?"Enter":(playstation?"X":"A"));
    }

    function styleCraftButton(action:MovieClip,label:String):Void {
        styleCommandButton(action,label,!gamepad?"C":(playstation?"SQR":"X"));
    }

    function send(command:String, index:Number, value:String):Void {
        if (data == undefined) return;
        if (command == "preview" || command == "clearPreview") {
            pendingPreview={command:command,index:index,revision:data.revision};
            return;
        }
        if (root.aosCommand != undefined)
            root.aosCommand(command,index,data.revision,asText(value));
    }

    function asText(v):String { return v == undefined ? "" : String(v); }
    function number(n:Number):String { return isNaN(n) ? "-" : String(Math.round(n)); }
    function weight(n:Number):String { return isNaN(n) ? "-" : String(Math.round(n*10)/10); }
    function gold(n:Number):String { return n+" "+T("gold"); }

    function saveView():Void {
        if (data == undefined) return;
        var key:String = matches[selected] == undefined ? "" : matches[selected].key;
        memory[data.key] = { category:category, group:group, sorting:sorting, descending:descending, search:search, key:key, offset:offset };
    }

    function computeAnchors():Void {
        leftShift=0; rightShift=0; viewX=0; viewY=0; viewW=1280; viewH=720;
        var stageObject:Object=_global["Stage"];
        var rect:Object=stageObject==undefined?undefined:stageObject["visibleRect"];
        if(rect!=undefined && rect.width>0 && rect.height>0) { viewX=rect.x; viewY=rect.y; viewW=rect.width; viewH=rect.height; }
        // Capped so very wide screens keep both sides close enough to read together.
        var extra:Number=Math.min(Math.max(0,(viewW-1280)/2),maxShift);
        leftShift=-extra; rightShift=extra;
    }

    // Centre of the 3D preview area, as a fraction of the screen, for the native renderer.
    function publishPreviewArea():Void {
        var cx:Number=947+rightShift; var cy:Number=232;
        var fx:Number=(cx-viewX)/viewW; var fy:Number=(cy-viewY)/viewH;
        send("previewArea",0,String(Math.round(fx*10000)/10000)+","+String(Math.round(fy*10000)/10000));
    }

    function stopRotation():Void {
        if(rotating) { rotating=false; send("rotateStop",0); }
    }

    function hasKnownCost():Boolean {
        for (var i:Number=0;i<data.rows.length;i++) if (data.rows[i].costDefined) return true;
        return false;
    }

    function configureSorts():Void {
        var layout:String = data.layout;
        sortLabels = [T("Name")];
        sortFields = ["name"];
        if (layout == "smithing-craft") {
            sortLabels = [T("Name"),T("Type"),T("Price")];
            sortFields = ["name","typeValue","price"];
        } else if (layout == "smithing-improve" || layout == "alchemy" || (layout == "enchanting-effects" && hasKnownCost())) {
            sortLabels = [T("Name"),T("Price")];
            sortFields = ["name","price"];
        } else if (layout == "enchanting-items") {
            sortLabels = [T("Name"),T("Type")];
            sortFields = ["name","typeValue"];
        } else if (layout == "orders") {
            sortLabels = [T("Name"),T("Status")];
            sortFields = ["name","statusValue"];
        }
    }

    function setPage(value:Object):Void {
        saveView();
        if (searching) send("textOff",0);
        if (canvas != undefined) canvas.removeMovieClip();
        modal = undefined;
        data = value;
        pendingMouse = undefined;
        pendingPreview = undefined;
        send("uiStage",0,"filters");
        busy = false; confirming = false; confirmationCommand = "activate"; renaming = false; renameValue = ""; renameOriginal = ""; renameBeforeEdit = ""; renameApplied = false;
        popupOpen = false; searching = false;
        configureSorts();

        var state:Object = memory[data.key];
        category = state == undefined ? "All" : state.category;
        group = state == undefined ? "All" : state.group;
        sorting = state == undefined ? 0 : state.sorting;
        descending = state != undefined && state.descending == true;
        search = state == undefined || state.search == undefined ? "" : state.search;
        if (sorting >= sortLabels.length) { sorting = 0; descending = false; }
        offset = 0; selected = 0;

        categories = ["All"]; groups = ["All"];
        for (var i:Number=0;i<data.rows.length;i++) {
            addUnique(categories,asText(data.rows[i].category));
            var parts:Array = asText(data.rows[i].group).split(" / ");
            for (var j:Number=0;j<parts.length;j++) addUnique(groups,parts[j]);
        }
        if (!data.showCategoryFilter || find(categories,category)<0) category = "All";
        if (!data.showGroupFilter || find(groups,group)<0) group = "All";

        send("uiStage",0,"panels");
        stopRotation();
        computeAnchors();
        var L:Number=leftShift; var R:Number=rightShift;
        canvas = root.createEmptyMovieClip("canvas",10);
        ArtisansDraw.panel(canvas,"leftPanel",34+L,24,600,614,82);
        ArtisansDraw.panel(canvas,"infoPanel",650+R,444,594,150,80);
        box(canvas,"bottom",34+L,648,1210+R-L,48,0x050505,88);
        ArtisansDraw.rule(canvas,"bottomTop",34+L,648,1210+R-L,20);

        // Drag here to rotate the item, like the inventory. Only while the
        // button is held, so plain mouse movement never turns the model.
        var self:ArtisansMenu=this;

        text(canvas,"artisan",data.artisan,62+L,42,544,38,28,white);
        text(canvas,"rank",(data.tier+" "+data.profession).toUpperCase(),64+L,80,540,24,16,warm);

        var y:Number = 118;
        if (isSmithing()) { drawServiceTabs(y); y += 46; }
        tabsTop = -1;
        if (data.showCategoryFilter) { tabsTop = y; y += 46; }
        searchTop = -1;
        if (data.layout != "smithing-quality") { searchTop = y; y += 46; }
        headerTop = y + 2;
        rowTop = headerTop + 28;
        visibleRows = Math.max(4,Math.floor((576-rowTop)/rowH));
        drawCategoryTabs();
        drawSearchBar();
        drawListHeader();

        list = canvas.createEmptyMovieClip("list",canvas.getNextHighestDepth());
        card = canvas.createEmptyMovieClip("card",canvas.getNextHighestDepth());
        send("uiStage",0,"list");
        filter(state == undefined ? "" : state.key);
        send("uiStage",0,"footer");
        if (state != undefined) {
            offset = Math.max(0,Math.min(state.offset,Math.max(0,matches.length-visibleRows)));
            if(selected<offset || selected>=offset+visibleRows) offset=Math.max(0,selected-visibleRows+1);
            drawList();
        }

        var funds:TextField=text(canvas,"funds",T("Gold")+"  "+data.gold,740+R,659,180,26,17,warm);
        var carry:TextField=text(canvas,"carry",T("Carry Weight")+"  "+number(data.carry)+"/"+number(data.capacity),940+R,659,270,26,17,white);
        align(funds,"right"); align(carry,"right");
        refreshButtons();
        publishPreviewArea();
        send("uiStage",0,"complete");
    }

    // Craft and Improve tabs: craft list, quality choice and improvement list.
    function isSmithing():Boolean {
        return data.layout == "smithing-craft" || data.layout == "smithing-improve" || data.layout == "smithing-quality";
    }

    function drawServiceTabs(y:Number):Void {
        var craft:Boolean = data.layout == "smithing-craft";
        var self:ArtisansMenu = this;
        var names:Array = [U("Craft"),U("Improve")];
        for (var i:Number=0;i<2;i++) {
            var active:Boolean = (i==0) == craft;
            var tab:MovieClip = box(canvas,"service"+i,60+leftShift+i*278,y,270,36,0xFFFFFF,active?6:0);
            var label:TextField = text(tab,"label",names[i],0,5,270,28,18,active?white:muted);
            label.wordWrap=false; align(label,"center");
            line(tab,"base",0,35,270,active?70:14);
            tab.useHandCursor=false;
            tab.goCraft = i==0;
            tab.onRelease=function():Void { if(this.goCraft != craft) self.queueMouse(function():Void { self.navigate("alternate"); }); };
        }
    }

    function categoryText(value:String):String { return value=="All"?T("All"):value; }

    function drawCategoryTabs():Void { ArtisansControls.tabs(this); }

    function selectCategory(index:Number):Void {
        if (confirming || busy || popupOpen || index<0 || index>=categories.length) return;
        if (searching) endSearch(false);
        category = categories[index];
        drawCategoryTabs();
        filter("");
    }

    function moveCategory(delta:Number):Void {
        if (tabsTop<0 || categories.length<2) return;
        var index:Number = find(categories,category);
        selectCategory((index+delta+categories.length)%categories.length);
    }

    function drawSearchBar():Void { ArtisansControls.searchBar(this); }

    function beginSearch():Void {
        if (searchTop<0 || searching || confirming || busy || popupOpen) return;
        searching = true;
        send("textOn",0);
        drawSearchBar(); drawHints();
    }

    function endSearch(clear:Boolean):Void {
        if (!searching) return;
        searching = false;
        send("textOff",0);
        Selection.setFocus(null);
        if (clear && search.length>0) { search=""; filter(""); }
        drawSearchBar(); drawHints();
    }

    function drawListHeader():Void { ArtisansControls.listHeader(this); }

    function setSort(index:Number,toggle:Boolean):Void {
        if (confirming || busy || index<0 || index>=sortFields.length || data.layout=="smithing-quality") return;
        if (index==sorting) { if (toggle) descending=!descending; }
        else { sorting=index; descending=false; }
        var key:String = matches[selected]==undefined?"":matches[selected].key;
        drawListHeader();
        filter(key);
    }

    function hasPopup():Boolean {
        return data.layout!="smithing-quality" && (sortFields.length>1 || (data.showGroupFilter && groups.length>1));
    }

    function drawHints():Void {
        if (canvas == undefined) return;
        if (canvas.hintBar != undefined) canvas.hintBar.removeMovieClip();
        var keys:Array=[]; var labels:Array=[]; var commands:Array=[];
        var accept:String=!gamepad?"Enter":(playstation?"X":"A");
        var back:String=!gamepad?"Esc":(playstation?"O":"B");
        if(searching) {
            keys=["Enter","Esc"]; labels=[T("Done"),T("Clear")]; commands=["acceptText","cancelText"];
        } else if(popupOpen) {
            keys=[accept,back,gamepad?"D-pad":"Wheel"]; labels=[T("Apply"),T("Close"),T("Choose")]; commands=["activate","back",""];
        } else {
            keys=[accept,back]; labels=[T("Select"),T("Back")]; commands=["activate","back"];
            if(data.layout=="enchanting-effects") { keys.push(!gamepad?"C":(playstation?"SQR":"X")); labels.push(T("Craft")); commands.push("craft"); }
            if(tabsTop>=0 && categories.length>1) { keys.push(gamepad?"D-pad":"Arrows"); labels.push(T("Category")); commands.push("right"); }
            if(hasPopup()) { keys.push(!gamepad?"F":(playstation?"TRI":"Y")); labels.push(T("Filters")); commands.push("filters"); }
            if(asText(data.alternate).length>0) {
                keys.push(!gamepad?"Tab":(playstation?"L1/R1":"LB/RB"));
                labels.push(isSmithing()?T("Craft / Improve"):asText(data.alternate)); commands.push("alternate");
            }
        }
        var availableWidth:Number=566;
        var hintBar:MovieClip=canvas.createEmptyMovieClip("hintBar",canvas.getNextHighestDepth());
        hintBar._x=46+leftShift; hintBar._y=660;
        var cursorX:Number=0;
        for(var i:Number=0;i<keys.length;i++) {
            var measure:TextField=text(hintBar,"measure",labels[i],0,0,250,24,15,muted);
            var promptW:Number=badgeWidth(keys[i])+8+measure.textWidth+10;
            measure.removeTextField();
            drawPrompt(hintBar,"prompt"+i,keys[i],labels[i],commands[i],cursorX,promptW);
            cursorX+=promptW+8;
        }
        cursorX-=8;
        if(cursorX>availableWidth) hintBar._xscale=availableWidth/cursorX*100;
    }

    function addUnique(values:Array,v:String):Void { if (v.length>0 && find(values,v)<0) values.push(v); }
    function find(values:Array,v:String):Number { for(var i:Number=0;i<values.length;i++) if(values[i]==v) return i; return -1; }

    function filter(wanted:String):Void {
        matches=[];
        var needle:String=search.toLowerCase();
        for(var i:Number=0;i<data.rows.length;i++) {
            var row:Object=data.rows[i];
            if(data.showCategoryFilter && category!="All" && row.category!=category) continue;
            if(data.showGroupFilter && group!="All" && find(asText(row.group).split(" / "),group)<0) continue;
            if(needle.length>0 && asText(row.name).toLowerCase().indexOf(needle)<0) continue;
            matches.push(row);
        }
        var field:String = sortFields[sorting];
        var reverse:Number = descending ? Array.DESCENDING : 0;
        // Use the VM's field sort, without repeatedly re-entering an AS2 comparator.
        // The final numeric id keeps equal names/prices in a deterministic order.
        // Qualities stay in strength order (Fine ... Legendary), as sent by the service.
        if(data.layout=="smithing-quality") matches.sortOn("id",Array.NUMERIC);
        else if(field=="name") matches.sortOn(["name","id"],[Array.CASEINSENSITIVE|reverse,Array.NUMERIC]);
        else matches.sortOn([field,"name","id"],[(field=="price"?Array.NUMERIC:Array.CASEINSENSITIVE)|reverse,Array.CASEINSENSITIVE,Array.NUMERIC]);
        selected=0;
        for(var k:Number=0;k<matches.length;k++) if(matches[k].key==wanted) selected=k;
        offset=Math.max(0,selected-visibleRows+1);
        drawList(); drawCard();
    }

    // Filters popup: sort order and the group filter (Material / Effect).
    function openPopup():Void {
        if(confirming || busy || popupOpen || !hasPopup()) return;
        if(searching) endSearch(false);
        popupOptions=[];
        popupIndex=-1;
        if(sortFields.length>1) {
            popupOptions.push({title:T("Sort by")});
            for(var i:Number=0;i<sortFields.length;i++) {
                if(i==sorting) popupIndex=popupOptions.length;
                popupOptions.push({kind:"sort",index:i,label:sortLabels[i]});
            }
        }
        if(data.showGroupFilter && groups.length>1) {
            popupOptions.push({title:asText(data.groupLabel)});
            for(var j:Number=0;j<groups.length;j++) {
                if(popupIndex<0 && groups[j]==group) popupIndex=popupOptions.length;
                popupOptions.push({kind:"group",value:groups[j],label:categoryText(groups[j])});
            }
        }
        if(popupIndex<0) popupIndex=1;
        popupOpen=true;
        stopRotation();
        drawPopup(); drawHints();
    }

    function closePopup():Void {
        popupOpen=false;
        if(canvas.popup!=undefined) canvas.popup.removeMovieClip();
        drawHints();
    }

    function drawPopup():Void { ArtisansControls.popup(this); }

    function movePopup(delta:Number):Void {
        var index:Number=popupIndex;
        do { index+=delta; } while(index>=0 && index<popupOptions.length && popupOptions[index].title!=undefined);
        if(index<0 || index>=popupOptions.length) return;
        popupIndex=index; drawPopup();
    }

    function applyPopup():Void {
        var option:Object=popupOptions[popupIndex];
        if(option==undefined || option.title!=undefined) return;
        closePopup();
        if(option.kind=="sort") { setSort(option.index,true); return; }
        group=option.value;
        drawSearchBar();
        filter("");
    }

    function put(parent:MovieClip,name:String,v:String,x:Number,w:Number,color:Number):Void {
        var field:TextField=text(parent,name,cap(v),x,5,w,28,18,color);
        field.wordWrap=false;
        while(field.textWidth>w-6 && field.text.length>4) field.text=field.text.substr(0,field.text.length-4)+"...";
    }

    function putCentered(parent:MovieClip,name:String,v:String,x:Number,w:Number,color:Number):Void {
        var field:TextField=text(parent,name,v,x,5,w,28,18,color);
        field.wordWrap=false;
        align(field,"center");
    }

    function priceColor(row:Object,chosen:Boolean):Number {
        if(row.picked) return picked;
        return row.costDefined && row.price>data.gold?danger:(chosen?warm:muted);
    }

    function drawList():Void {
        if (list != undefined) list.removeMovieClip();
        list=canvas.createEmptyMovieClip("list",canvas.getNextHighestDepth());
        list._x=leftShift;
        var self:ArtisansMenu=this;
        for(var i:Number=0;i<visibleRows && offset+i<matches.length;i++) {
            var row:Object=matches[offset+i];
            var chosen:Boolean=selected==offset+i;
            var mc:MovieClip=box(list,"r"+i,60,rowTop+i*rowH,548,rowH-1,chosen?0x34383D:0xFFFFFF,chosen?58:0);
            mc.rowIndex=offset+i;
            mc.useHandCursor=false;
            // Mouse: hovering selects, clicking the selected row uses it.
            mc.onRollOver=function():Void {
                var index:Number=this.rowIndex;
                if(self.wheelLock) return;
                self.queueMouse(function():Void { if(!self.confirming && !self.popupOpen && !self.wheelLock) self.choose(index); });
            };
            mc.onRelease=function():Void {
                var index:Number=this.rowIndex;
                self.queueMouse(function():Void {
                    if(self.confirming || self.busy || self.popupOpen) return;
                    if(self.searching) self.endSearch(false);
                    if(self.selected==index) self.activate(); else self.choose(index);
                });
            };
            if (chosen) box(mc,"selection",0,0,3,rowH-1,warm,row.picked?40:85);
            if (row.picked) box(mc,"pickedMark",0,0,3,rowH-1,picked,90);
            var base:Number=row.picked?picked:(row.enabled?white:muted);
            var layout:String=data.layout;
            var priced:Boolean=layout=="smithing-craft" || layout=="smithing-improve" || layout=="enchanting-effects" || layout=="alchemy";
            if(layout=="smithing-craft") {
                put(mc,"name",row.name,10,290,base); put(mc,"type",row.typeValue,310,165,chosen?warm:muted);
            } else if(layout=="smithing-improve") {
                put(mc,"name",row.name,10,220,base); put(mc,"current",row.currentQuality,235,88,muted); put(mc,"result",row.resultQuality,330,95,chosen?warm:muted);
            } else if(layout=="enchanting-items") {
                put(mc,"name",row.name,10,335,base); put(mc,"type",row.typeValue,350,185,chosen?warm:muted);
            } else if(layout=="orders") {
                put(mc,"name",row.name,10,240,base); put(mc,"status",row.statusValue,255,95,chosen?warm:muted); put(mc,"time",row.timeValue,360,175,muted);
            } else if(layout=="enchanting-effects" || layout=="alchemy" || layout=="smithing-quality") {
                put(mc,"name",row.name,10,410,base);
            } else {
                put(mc,"name",row.name,10,410,base); priced=true;
            }
            if(priced) putCentered(mc,"price",row.costDefined?String(row.price):"-",476,72,priceColor(row,chosen));
        }
        if(matches.length==0) text(list,"empty",T("No matching items."),62,rowTop+8,540,30,18,muted);
    }

    // Card columns share the 540 px inner width evenly.
    function columnX(index:Number,count:Number):Number { return 700+index*(columnW(count)+16); }
    function columnW(count:Number):Number { return (500-16*(count-1))/count; }

    function info(parent:MovieClip,name:String,label:String,v:String,x:Number,y:Number,w:Number,highlight:Boolean):Void {
        if (asText(v).length==0) return;
        var labelField:TextField=text(parent,name+"Label",asText(label).toUpperCase(),x,y,w,20,14,muted);
        var valueField:TextField=text(parent,name+"Value",cap(v),x,y+20,w,60,19,highlight?warm:white);
        labelField.wordWrap=false;
        align(labelField,"left"); align(valueField,"left");
    }

    function drawCard():Void {
        if (card != undefined) card.removeMovieClip();
        card=canvas.createEmptyMovieClip("card",canvas.getNextHighestDepth());
        card._x=rightShift;
        var row:Object=matches[selected];
        drawNotice();
        // Quality choice: nothing to show on the card or in the preview.
        if(row==undefined || data.layout=="smithing-quality") { send("clearPreview",0); return; }
        send("preview",row.id);
        var cardName:TextField=text(card,"name",row.name+(data.layout=="alchemy"?" (x"+row.quantity+")":""),700,458,500,30,22,white);
        var layout:String=data.layout;
        var affordable:Boolean=row.price<=data.gold;
        if(layout=="smithing-craft") {
            info(card,"stat",asText(row.statLabel),number(row.stat),columnX(0,4),502,columnW(4),true);
            info(card,"weight",T("Weight"),weight(row.weight),columnX(1,4),502,columnW(4),false);
            if(row.costDefined) info(card,"price",T("Price"),gold(row.price),columnX(2,4),502,columnW(4),affordable);
            info(card,"delivery",T("Delivery"),row.deliveryValue,columnX(3,4),502,columnW(4),false);
        } else if(layout=="smithing-improve") {
            info(card,"current",asText(row.statLabel),number(row.stat)+" > "+number(row.resultStat),columnX(0,3),502,columnW(3),false);
            info(card,"result",T("Result Quality"),row.resultQuality,columnX(1,3),502,columnW(3),true);
            if(row.costDefined) info(card,"price",T("Price"),gold(row.price),columnX(2,3),502,columnW(3),affordable);
        } else if(layout=="enchanting-items") {
            var hasStat:Boolean=asText(row.statLabel).length>0;
            if(hasStat) info(card,"stat",asText(row.statLabel),number(row.stat),columnX(0,2),502,columnW(2),true);
            info(card,"weight",T("Weight"),weight(row.weight),columnX(hasStat?1:0,2),502,columnW(2),false);
        } else if(layout=="enchanting-effects" || layout=="alchemy") {
            cardName._width=380;
            if(row.costDefined) info(card,"price",T("Price"),gold(row.price),1090,456,110,affordable);
            info(card,"effect",T("Effect"),row.effectValue,700,500,500,true);
        } else if(layout=="orders") {
            info(card,"status",T("Status"),row.statusValue,columnX(0,3),502,columnW(3),true);
            info(card,"time",T("Time left"),row.timeValue,columnX(1,3),502,columnW(3),false);
            info(card,"pickup",T("Pickup"),row.pickupValue,columnX(2,3),502,columnW(3),false);
        }
        var self:ArtisansMenu=this;
        var actionX:Number=layout=="enchanting-effects"?730:847;
        var action:MovieClip=button(card,"action",row.action,actionX,602,200,function():Void { self.activate(); });
        styleActionButton(action,row.action);
        if(!row.enabled || (row.confirm && row.costDefined && row.price>data.gold)) { action._alpha=38; action.enabled=false; }
        if(layout=="enchanting-effects") {
            var craft:MovieClip=button(card,"craft",finalizeText(),960,602,200,function():Void { self.craftSelection(); });
            styleCraftButton(craft,finalizeText());
            if(!data.canFinalize || data.finalizePrice>data.gold) { craft._alpha=38; craft.enabled=false; }
        }
    }

    function drawNotice():Void {
        if(canvas.notice!=undefined) canvas.notice.removeTextField();
        var row:Object=matches[selected];
        var message:String=asText(data.notice);
        if(row!=undefined && asText(row.blocked).length>0) message+=(message.length>0?"\n":"")+row.blocked;
        if(message.length>0) text(canvas,"notice",capLines(message),62+leftShift,580,546,56,16,white);
    }

    function wheel(delta:Number):Void {
        if(confirming || busy || gamepad) return;
        // Works anywhere on the menu; hovering does not undo it until the mouse moves.
        wheelLock=true; lockX=canvas._xmouse; lockY=canvas._ymouse;
        if(popupOpen) movePopup(delta);
        else move(delta);
    }

    function move(delta:Number):Void {
        if(matches.length==0 || confirming || busy || popupOpen) return;
        selected=Math.max(0,Math.min(matches.length-1,selected+delta));
        if(selected<offset) offset=selected;
        if(selected>=offset+visibleRows) offset=selected-visibleRows+1;
        drawList(); drawCard();
    }

    function choose(index:Number):Void {
        if(confirming || busy || popupOpen || index==selected) return;
        selected=index; drawList(); drawCard();
    }

    function openModal(x:Number,y:Number,w:Number,h:Number):Void {
        confirming=true; stopRotation(); send("clearPreview",0);
        if(searching) endSearch(false);
        // The dim layer covers the whole visible screen; panels stay centred.
        modal=canvas.createEmptyMovieClip("modal",canvas.getNextHighestDepth());
        box(modal,"dim",viewX,viewY,viewW,viewH,0x000000,78);
        var shield:MovieClip=box(modal,"shield",viewX,viewY,viewW,viewH,0,0);
        shield.onRelease=function():Void {};
        shield.useHandCursor=false;
        ArtisansDraw.panel(modal,"panel",x,y,w,h,97);
    }

    function craftSelection():Void {
        if(busy || confirming || popupOpen || data.layout!="enchanting-effects" || !data.canFinalize || data.finalizePrice>data.gold) return;
        confirmationCommand="finalize";
        renaming=false;
        renameOriginal=asText(data.finalizeItem);
        renameValue=renameOriginal;
        renameBeforeEdit=renameValue;
        renameApplied=false;
        openModal(310,158,660,406);

        var heading:TextField=text(modal,"heading",U("Craft"),350,177,580,30,21,warm);
        var item:TextField=text(modal,"item",asText(data.finalizeItem),350,216,580,35,22,white);
        align(heading,"center"); align(item,"center");

        text(modal,"oneLabel",U("Enchantment 1"),350,256,240,20,14,dim);
        text(modal,"oneName",asText(data.selectionOneName),350,278,390,25,18,picked);
        align(text(modal,"onePrice",gold(data.selectionOnePrice),750,278,180,25,18,warm),"right");
        text(modal,"oneEffect",cap(data.selectionOneEffect),350,304,580,44,16,muted);

        if(asText(data.selectionTwoName).length>0) {
            text(modal,"twoLabel",U("Enchantment 2"),350,348,240,20,14,dim);
            text(modal,"twoName",asText(data.selectionTwoName),350,370,390,25,18,picked);
            align(text(modal,"twoPrice",gold(data.selectionTwoPrice),750,370,180,25,18,warm),"right");
            text(modal,"twoEffect",cap(data.selectionTwoEffect),350,396,580,44,16,muted);
        }

        ArtisansDraw.rule(modal,"totalLine",350,442,580,18);
        align(text(modal,"total",T("Total")+"   "+gold(data.finalizePrice),350,454,580,30,19,warm),"center");
        ArtisansDraw.rule(modal,"actionsLine",350,491,580,16);
        var self:ArtisansMenu=this;
        button(modal,"cancel",cancelText(),350,507,170,function():Void { self.cancelModal(); });
        button(modal,"rename",renameText(),555,507,170,function():Void { self.beginRename(); });
        button(modal,"confirm",confirmText(),760,507,170,function():Void { self.confirmModal(); });
        updateModalLabels();
    }

    function trimName(value:String):String {
        var start:Number=0; var finish:Number=value.length;
        while(start<finish && value.charCodeAt(start)<=32) start++;
        while(finish>start && value.charCodeAt(finish-1)<=32) finish--;
        return value.substring(start,finish);
    }

    function beginRename():Void {
        if(!confirming || confirmationCommand!="finalize" || renaming || busy || modal==undefined || modal.item==undefined) return;
        renaming=true;
        renameBeforeEdit=renameValue;
        var field:TextField=modal.item;
        field.type="input";
        field.selectable=true;
        field.multiline=false;
        field.wordWrap=false;
        field.maxChars=40;
        field.text=renameValue;
        field.border=true;
        field.borderColor=warm;
        field.background=true;
        field.backgroundColor=0x111315;
        align(field,"left");
        modal.heading.text=U("Rename item");
        Selection.setFocus(field);
        Selection.setSelection(0,field.text.length);
        send("textOn",0);
        updateModalLabels();
    }

    function leaveRename(value:String):Void {
        renameValue=value.length>0?value:renameBeforeEdit;
        renameApplied=renameValue!=renameOriginal;
        renaming=false;
        send("textOff",0);
        Selection.setFocus(null);
        var field:TextField=modal.item;
        field.type="dynamic";
        field.selectable=false;
        field.multiline=true;
        field.wordWrap=true;
        field.border=false;
        field.background=false;
        field.text=renameValue;
        align(field,"center");
        modal.heading.text=U("Craft");
        updateModalLabels();
    }

    function acceptRename():Void {
        if(!renaming || modal==undefined || modal.item==undefined) return;
        leaveRename(trimName(modal.item.text));
    }

    function cancelRename():Void {
        if(!renaming) return;
        leaveRename(renameBeforeEdit);
    }

    function confirmModal():Void { if(renaming) acceptRename(); else finish(); }
    function cancelModal():Void { if(renaming) cancelRename(); else cancel(); }

    function activate():Void {
        if(busy || popupOpen) return;
        if(confirming) { finish(); return; }
        var row:Object=matches[selected];
        if(row==undefined || !row.enabled) return;
        if(row.confirm && row.costDefined && row.price>data.gold) return;
        if(searching) endSearch(false);
        Selection.setFocus(null);
        if(!row.confirm) { busy=true; send("activate",row.id); return; }
        confirmationCommand="activate";
        openModal(350,226,580,258);
        align(text(modal,"heading",asText(row.action).toUpperCase(),390,246,500,30,21,warm),"center");
        align(text(modal,"item",row.name+(row.quantity>1?"   x"+row.quantity:""),390,292,500,50,23,white),"center");
        if(row.costDefined) align(text(modal,"price",T("Price")+"   "+gold(row.price),390,345,500,30,18,warm),"center");
        if(data.layout=="smithing-craft" && row.deliveryValue!=T("Immediate"))
            align(text(modal,"summary",T("Ready in")+"   "+row.deliveryValue,390,378,500,26,18,white),"center");
        ArtisansDraw.rule(modal,"actionsLine",390,410,500,16);
        var self:ArtisansMenu=this;
        button(modal,"cancel",cancelText(),390,428,230,function():Void { self.cancelModal(); });
        button(modal,"confirm",confirmText(),660,428,230,function():Void { self.confirmModal(); });
        updateModalLabels();
    }

    function confirmText():String {
        var row:Object=matches[selected];
        var label:String=confirmationCommand=="finalize"?asText(data.finalizeLabel):(row==undefined?"":asText(row.action));
        return label.length==0?T("Confirm"):label;
    }
    function cancelText():String { return T("Cancel"); }
    function renameText():String { return renaming?T("Editing..."):T("Rename"); }
    function updateModalLabels():Void {
        if(modal==undefined) return;
        if(modal.confirm!=undefined) styleCommandButton(modal.confirm,confirmText(),!gamepad?"Enter":(playstation?"X":"A"));
        if(modal.cancel!=undefined) styleCommandButton(modal.cancel,cancelText(),!gamepad?"Esc":(playstation?"O":"B"));
        if(modal.rename!=undefined) { styleCommandButton(modal.rename,renameText(),!gamepad?"R":(playstation?"SQR":"X")); modal.rename._alpha=renaming?45:100; }
    }

    function finish():Void { if(renaming) { acceptRename(); return; } if(confirming && !busy) { busy=true; send("textOff",0); styleCommandButton(modal.confirm,T("Please wait..."),""); send(confirmationCommand,confirmationCommand=="finalize"?0:matches[selected].id,renameApplied?renameValue:""); } }
    function cancel():Void { if(renaming) { cancelRename(); return; } if(!busy) { send("textOff",0); confirming=false; confirmationCommand="activate"; modal.removeMovieClip(); drawCard(); } }

    function navigate(command:String):Void {
        if(data==undefined || busy) return;
        if(command=="up" || command=="down" || command=="left" || command=="right") {
            var now:Number=getTimer();
            if(command==lastNavCommand && now-lastNavAt<90) return;
            lastNavCommand=command; lastNavAt=now;
        }
        if(command=="close") { stopRotation(); saveView(); send("textOff",0); send("close",0); return; }
        if(searching) {
            if(command=="acceptText" || command=="activate") endSearch(false);
            else if(command=="cancelText" || command=="back") endSearch(true);
            return;
        }
        if(renaming) {
            if(command=="back" || command=="cancelText") cancelRename();
            else if(command=="activate" || command=="acceptText") acceptRename();
            return;
        }
        if(confirming) {
            if(command=="back") cancel();
            else if(command=="activate") finish();
            else if(command=="craft" || command=="rename") beginRename();
            return;
        }
        if(command=="wheelUp" || command=="wheelDown") { wheel(command=="wheelUp"?-1:1); return; }
        if(popupOpen) {
            if(command=="up") movePopup(-1); else if(command=="down") movePopup(1);
            else if(command=="activate") applyPopup();
            else if(command=="back" || command=="filters") closePopup();
            return;
        }
        if(command=="filters") { openPopup(); return; }
        if(command=="left" || command=="right") { moveCategory(command=="left"?-1:1); return; }
        if(command=="up") move(-1); else if(command=="down") move(1); else if(command=="activate") activate();
        else if(command=="craft") craftSelection();
        else if((command=="alternate" || command=="previousMode" || command=="nextMode") && asText(data.alternate).length>0) { saveView(); busy=true; send("alternate",0); }
        else if(command=="back") { saveView(); if(data.hasBack) { busy=true; send("back",0); } else send("close",0); }
    }
}
