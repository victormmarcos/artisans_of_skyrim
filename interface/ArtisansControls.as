// List controls of ArtisansMenu: category tabs, search field, sortable column
// titles and the filters popup. Separate class so each stays under AVM1's
// 32 KiB per-class bytecode comfort zone.
class ArtisansControls {
    static function text(m:ArtisansMenu, parent:MovieClip, name:String, value:String, x:Number, y:Number, w:Number, h:Number, size:Number, color:Number):TextField {
        var field:TextField = ArtisansDraw.text(parent,name,value,x,y,w,h,size,color,m.font);
        field.wordWrap = false;
        return field;
    }

    static function line(parent:MovieClip, name:String, x:Number, y:Number, w:Number, alpha:Number):Void {
        ArtisansDraw.box(parent,name,x,y,w,1,0xFFFFFF,alpha);
    }

    // Category tabs (left/right or click), like the inventory columns.
    static function tabs(m:ArtisansMenu):Void {
        var canvas:MovieClip = m.canvas;
        if (canvas.tabs != undefined) canvas.tabs.removeMovieClip();
        if (m.tabsTop < 0) return;
        var bar:MovieClip = canvas.createEmptyMovieClip("tabs",canvas.getNextHighestDepth());
        bar._x = 60+m.leftShift; bar._y = m.tabsTop;
        var captionText:String = m.asText(m.data.categoryLabel);
        var caption:TextField = text(m,bar,"caption",captionText.toUpperCase(),0,5,160,28,17,m.dim);
        var startX:Number = captionText.length>0 ? Math.min(170,caption.textWidth+22) : 0;
        caption._width = Math.max(1,startX);
        var width:Number = 548-startX;
        var area:MovieClip = bar.createEmptyMovieClip("area",bar.getNextHighestDepth());
        area._x = startX;
        var strip:MovieClip = area.createEmptyMovieClip("strip",area.getNextHighestDepth());
        var x:Number = 0; var activeX:Number = 0; var activeW:Number = 0;
        for (var i:Number=0;i<m.categories.length;i++) {
            var active:Boolean = m.categories[i]==m.category;
            var tab:MovieClip = ArtisansDraw.box(strip,"t"+i,x,0,10,34,0,0);
            var label:TextField = text(m,tab,"label",m.categoryText(m.categories[i]).toUpperCase(),0,5,400,28,17,active?m.white:m.muted);
            var w:Number = label.textWidth+26;
            label._width = w; ArtisansDraw.align(label,"center");
            ArtisansDraw.box(tab,"hit",0,0,w,34,0,0);
            if (active) { line(tab,"mark",8,32,w-16,80); activeX=x; activeW=w; }
            tab.categoryIndex = i;
            tab.owner = m;
            tab.useHandCursor=false;
            tab.onRelease=function():Void {
                var index:Number=this.categoryIndex; var menu:ArtisansMenu=this.owner;
                menu.queueMouse(function():Void { menu.selectCategory(index); });
            };
            x += w;
        }
        if (x > width) {
            var wanted:Number = activeX+activeW/2-width/2;
            strip._x = -Math.max(0,Math.min(x-width,wanted));
            strip.setMask(ArtisansDraw.box(area,"mask",0,0,width,35,0,100));
            if (strip._x < 0) text(m,bar,"more0","<",startX-12,6,12,28,17,m.dim);
            if (strip._x+x > width) text(m,bar,"more1",">",548,6,12,28,17,m.dim);
        }
        line(bar,"base",0,34,548,12);
    }

    // Search field (click to type) and the group filter chip next to it.
    static function searchBar(m:ArtisansMenu):Void {
        var canvas:MovieClip = m.canvas;
        if (canvas.searchBar != undefined) canvas.searchBar.removeMovieClip();
        if (m.searchTop < 0) return;
        var bar:MovieClip = canvas.createEmptyMovieClip("searchBar",canvas.getNextHighestDepth());
        bar._x = 60+m.leftShift; bar._y = m.searchTop;
        var chip:Boolean = m.data.showGroupFilter && m.groups.length>1;
        var w:Number = chip ? 330 : 548;
        var field:MovieClip = ArtisansDraw.box(bar,"field",0,0,w,32,0x000000,m.searching?70:40);
        line(field,"base",0,31,w,m.searching?60:18);
        ArtisansDraw.magnifier(field,8,8,m.searching?m.white:m.dim);
        var empty:Boolean = m.search.length==0 && !m.searching;
        var input:TextField = text(m,field,"input",empty?m.T("Search"):m.search,30,4,w-38,26,17,empty?m.dim:m.white);
        input.multiline=false;
        if (m.searching) {
            input.type="input"; input.selectable=true; input.maxChars=40;
            input.onChanged=function():Void { m.search=this.text; m.filter(""); };
            Selection.setFocus(input);
            Selection.setSelection(input.text.length,input.text.length);
        } else {
            field.useHandCursor=false;
            field.onRelease=function():Void { m.queueMouse(function():Void { m.beginSearch(); }); };
        }
        if (chip) {
            var groupButton:MovieClip = ArtisansDraw.box(bar,"group",338,0,210,32,0x000000,40);
            line(groupButton,"base",0,31,210,18);
            var label:TextField = text(m,groupButton,"label",m.asText(m.data.groupLabel)+": "+m.categoryText(m.group),8,4,194,26,17,m.group=="All"?m.muted:m.white);
            ArtisansDraw.align(label,"center");
            groupButton.useHandCursor=false;
            groupButton.onRelease=function():Void { m.queueMouse(function():Void { m.openPopup(); }); };
        }
    }

    // One column title; sortable ones are clicked to sort (again to reverse).
    static function header(m:ArtisansMenu,parent:MovieClip,label:String,x:Number,w:Number,centered:Boolean,field:String):Void {
        var left:Number=x+m.leftShift;
        var title:TextField=text(m,parent,"head"+x,label.toUpperCase(),left,m.headerTop-2,w,22,15,m.muted);
        if(centered) ArtisansDraw.align(title,"center");
        var index:Number=m.find(m.sortFields,field);
        if(index<0) return;
        if(index==m.sorting) {
            title.textColor=m.white;
            var textEnd:Number=centered?left+(w+title.textWidth)/2:left+title.textWidth+4;
            ArtisansDraw.arrow(parent,"sortMark",textEnd+6,m.headerTop+8,m.descending,m.warm);
        }
        var hit:MovieClip=ArtisansDraw.box(parent,"hit"+x,left,m.headerTop-2,w,24,0,0);
        hit.useHandCursor=false;
        hit.onRelease=function():Void { m.queueMouse(function():Void { m.setSort(index,true); }); };
    }

    static function listHeader(m:ArtisansMenu):Void {
        var canvas:MovieClip = m.canvas;
        if (canvas.headers != undefined) canvas.headers.removeMovieClip();
        var h:MovieClip=canvas.createEmptyMovieClip("headers",canvas.getNextHighestDepth());
        var layout:String=m.data.layout;
        if (layout=="smithing-craft") { header(m,h,m.T("Item"),70,290,false,"name"); header(m,h,m.T("Type"),370,165,false,"typeValue"); header(m,h,m.T("Price"),536,72,true,"price"); }
        else if (layout=="smithing-improve") { header(m,h,m.T("Item"),70,220,false,"name"); header(m,h,m.T("Current"),300,86,false,""); header(m,h,m.T("Result"),396,86,false,""); header(m,h,m.T("Price"),536,72,true,"price"); }
        else if (layout=="enchanting-items") { header(m,h,m.T("Item"),70,335,false,"name"); header(m,h,m.T("Type"),420,178,false,"typeValue"); }
        else if (layout=="enchanting-effects") { header(m,h,m.T("Enchantment"),70,410,false,"name"); header(m,h,m.T("Price"),536,72,true,"price"); }
        else if (layout=="alchemy") { header(m,h,m.T("Item"),70,410,false,"name"); header(m,h,m.T("Price"),536,72,true,"price"); }
        else if (layout=="orders") { header(m,h,m.T("Item"),70,240,false,"name"); header(m,h,m.T("Status"),315,90,false,"statusValue"); header(m,h,m.T("Time left"),420,120,false,""); }
        else if (layout=="smithing-quality") { header(m,h,m.T("Quality"),70,410,false,""); }
        else { header(m,h,m.T("Item"),70,360,false,"name"); header(m,h,m.T("Price"),536,72,true,"price"); }
        ArtisansDraw.rule(h,"listHeaderLine",60+m.leftShift,m.headerTop+21,548,16);
    }

    // Filters popup: sort order and the group filter (Material / Effect).
    static function popup(m:ArtisansMenu):Void {
        var canvas:MovieClip = m.canvas;
        if(canvas.popup!=undefined) canvas.popup.removeMovieClip();
        if(!m.popupOpen) return;
        var top:Number=m.headerTop-6;
        var height:Number=576-top;
        var panel:MovieClip=ArtisansDraw.panel(canvas,"popup",54+m.leftShift,top,560,height,97);
        var options:Array=m.popupOptions;
        var shown:Number=Math.floor((height-20)/34);
        var start:Number=Math.max(0,Math.min(m.popupIndex-Math.floor(shown/2),options.length-shown));
        for(var i:Number=0;i<shown && start+i<options.length;i++) {
            var index:Number=start+i;
            var option:Object=options[index];
            var y:Number=10+i*34;
            if(option.title!=undefined) {
                text(m,panel,"title"+i,String(option.title).toUpperCase(),24,y+8,512,22,14,m.dim);
                line(panel,"titleLine"+i,24,y+31,512,14);
                continue;
            }
            var active:Boolean=index==m.popupIndex;
            var row:MovieClip=ArtisansDraw.box(panel,"o"+i,14,y,532,32,0x34383D,active?70:1);
            text(m,row,"label",option.label,24,3,440,28,18,active?m.white:m.muted);
            var current:Boolean=option.kind=="sort"?option.index==m.sorting:option.value==m.group;
            if(current) ArtisansDraw.diamond(row,511,16,4,m.picked,95);
            row.optionIndex=index;
            row.useHandCursor=false;
            row.onRollOver=function():Void {
                var hovered:Number=this.optionIndex;
                m.queueMouse(function():Void { if(m.popupOpen && m.popupIndex!=hovered) { m.popupIndex=hovered; ArtisansControls.popup(m); } });
            };
            row.onRelease=function():Void {
                var chosen:Number=this.optionIndex;
                m.queueMouse(function():Void { m.popupIndex=chosen; m.applyPopup(); });
            };
        }
    }
}
