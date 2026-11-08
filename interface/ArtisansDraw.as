// Low-level drawing helpers for ArtisansMenu. Kept in a separate class so the
// menu class stays under AVM1's 32 KiB per-class bytecode comfort zone.
class ArtisansDraw {
    static function box(parent:MovieClip, name:String, x:Number, y:Number, w:Number, h:Number, color:Number, alpha:Number):MovieClip {
        var mc:MovieClip = parent.createEmptyMovieClip(name, parent.getNextHighestDepth());
        mc._x = x; mc._y = y;
        mc.beginFill(color, alpha);
        mc.moveTo(0,0); mc.lineTo(w,0); mc.lineTo(w,h); mc.lineTo(0,h); mc.lineTo(0,0);
        mc.endFill();
        return mc;
    }

    static function text(parent:MovieClip, name:String, value:String, x:Number, y:Number, w:Number, h:Number, size:Number, color:Number, font:String):TextField {
        parent.createTextField(name, parent.getNextHighestDepth(), x, y, w, h);
        var t:TextField = parent[name];
        t.text = value == undefined ? "" : value;
        t.selectable = false; t.multiline = true; t.wordWrap = true;
        var format:TextFormat = new TextFormat(font, size, color);
        format.leading = 3;
        t.setTextFormat(format); t.setNewTextFormat(format);
        return t;
    }

    static function align(field:TextField, how:String):Void {
        var format:TextFormat = field.getTextFormat(); format.align = how;
        field.setTextFormat(format); field.setNewTextFormat(format);
    }

    // Search icon: a small ring with a handle.
    static function magnifier(parent:MovieClip, x:Number, y:Number, color:Number):Void {
        var mc:MovieClip = parent.createEmptyMovieClip("magnifier", parent.getNextHighestDepth());
        mc._x = x; mc._y = y;
        mc.lineStyle(1.5, color, 90);
        mc.moveTo(11,6);
        for (var i:Number=1;i<=8;i++) {
            var end:Number=i*Math.PI/4; var mid:Number=end-Math.PI/8; var r:Number=5/Math.cos(Math.PI/8);
            mc.curveTo(6+r*Math.cos(mid),6+r*Math.sin(mid),6+5*Math.cos(end),6+5*Math.sin(end));
        }
        mc.moveTo(10,10); mc.lineTo(15,15);
    }

    // Sort direction mark: up for ascending, down for descending.
    static function arrow(parent:MovieClip, name:String, x:Number, y:Number, down:Boolean, color:Number):Void {
        var mc:MovieClip = parent.createEmptyMovieClip(name, parent.getNextHighestDepth());
        mc._x = x; mc._y = y;
        mc.beginFill(color, 90);
        if (down) { mc.moveTo(0,-2); mc.lineTo(8,-2); mc.lineTo(4,3); mc.lineTo(0,-2); }
        else { mc.moveTo(0,3); mc.lineTo(8,3); mc.lineTo(4,-2); mc.lineTo(0,3); }
        mc.endFill();
    }

    // ---------------------------------------------------------------
    // Skyrim-style frames: dark panel, thin double border, ornamented
    // corners and a small mark in the middle of the top edge.
    // ---------------------------------------------------------------
    static var ornament:Number = 0xC9C2B0;

    static function diamond(mc:MovieClip, x:Number, y:Number, r:Number, color:Number, alpha:Number):Void {
        mc.lineStyle();
        mc.beginFill(color, alpha);
        mc.moveTo(x, y-r); mc.lineTo(x+r, y); mc.lineTo(x, y+r); mc.lineTo(x-r, y); mc.lineTo(x, y-r);
        mc.endFill();
    }

    // Greek-key corner like the Journal: an L bracket with a square spiral.
    static function corner(mc:MovieClip, cx:Number, cy:Number, sx:Number, sy:Number, alpha:Number):Void {
        var c:Number = ornament;
        mc.lineStyle(1.5, c, alpha);
        mc.moveTo(cx, cy+sy*26); mc.lineTo(cx, cy); mc.lineTo(cx+sx*26, cy);
        mc.lineStyle(1.25, c, alpha*0.85);
        mc.moveTo(cx+sx*5, cy+sy*20); mc.lineTo(cx+sx*5, cy+sy*5); mc.lineTo(cx+sx*19, cy+sy*5);
        mc.lineTo(cx+sx*19, cy+sy*14); mc.lineTo(cx+sx*11, cy+sy*14); mc.lineTo(cx+sx*11, cy+sy*10);
    }

    static function panel(parent:MovieClip, name:String, x:Number, y:Number, w:Number, h:Number, alpha:Number):MovieClip {
        var mc:MovieClip = box(parent, name, x, y, w, h, 0x050505, alpha);
        var f:MovieClip = mc.createEmptyMovieClip("frame", mc.getNextHighestDepth());
        f.lineStyle(1, 0xFFFFFF, 20);
        f.moveTo(0,0); f.lineTo(w,0); f.lineTo(w,h); f.lineTo(0,h); f.lineTo(0,0);
        f.lineStyle(1, 0xFFFFFF, 9);
        f.moveTo(4,4); f.lineTo(w-4,4); f.lineTo(w-4,h-4); f.lineTo(4,h-4); f.lineTo(4,4);
        corner(f, 0, 0, 1, 1, 75); corner(f, w, 0, -1, 1, 75);
        corner(f, 0, h, 1, -1, 75); corner(f, w, h, -1, -1, 75);
        // Top centre mark.
        f.lineStyle(1, ornament, 55);
        f.moveTo(w/2-40, 0); f.lineTo(w/2-9, 0); f.moveTo(w/2+9, 0); f.lineTo(w/2+40, 0);
        diamond(f, w/2, 0, 5, ornament, 85);
        diamond(f, w/2, 0, 2, 0x050505, 100);
        return mc;
    }

    // Divider: thin double line, like the panel borders.
    static function rule(parent:MovieClip, name:String, x:Number, y:Number, w:Number, alpha:Number):Void {
        var mc:MovieClip = parent.createEmptyMovieClip(name, parent.getNextHighestDepth());
        mc._x = x; mc._y = y;
        mc.lineStyle(1, 0xFFFFFF, alpha);
        mc.moveTo(0, 0); mc.lineTo(w, 0);
        mc.lineStyle(1, 0xFFFFFF, alpha*0.45);
        mc.moveTo(0, 3); mc.lineTo(w, 3);
    }

    // ---------------------------------------------------------------
    // Button icons like the game: key caps on keyboard, coloured face
    // buttons on Xbox, shape symbols on PlayStation.
    // ---------------------------------------------------------------
    static var playstation:Boolean = false;

    static function isFace(key:String):Boolean {
        return key=="A" || key=="B" || key=="X" || key=="Y" || key=="O" || key=="SQR" || key=="TRI";
    }

    static function isIcon(key:String):Boolean {
        return key=="D-pad" || key=="Arrows" || key=="Wheel";
    }

    static function capWidth(key:String):Number {
        if (isFace(key) || isIcon(key) || key=="Enter") return 24;
        return Math.max(24, 12+key.length*8);
    }

    static function badgeWidth(key:String):Number {
        if (key.indexOf("/")>=0) { var keys:Array=key.split("/"); return capWidth(keys[0])+capWidth(keys[1])+6; }
        return capWidth(key);
    }

    static function roundRect(mc:MovieClip, w:Number, h:Number, r:Number):Void {
        mc.moveTo(r,0); mc.lineTo(w-r,0); mc.curveTo(w,0,w,r); mc.lineTo(w,h-r); mc.curveTo(w,h,w-r,h);
        mc.lineTo(r,h); mc.curveTo(0,h,0,h-r); mc.lineTo(0,r); mc.curveTo(0,0,r,0);
    }

    static function circle(mc:MovieClip, cx:Number, cy:Number, r:Number):Void {
        mc.moveTo(cx+r,cy);
        for(var i:Number=1;i<=8;i++) {
            var end:Number=i*Math.PI/4; var mid:Number=end-Math.PI/8; var cr:Number=r/Math.cos(Math.PI/8);
            mc.curveTo(cx+cr*Math.cos(mid),cy+cr*Math.sin(mid),cx+r*Math.cos(end),cy+r*Math.sin(end));
        }
    }

    static function keyLabel(mc:MovieClip, key:String, w:Number, size:Number, color:Number, font:String):Void {
        var keyText:TextField=text(mc,"key",key,0,1,w,22,size,color,font);
        keyText.wordWrap=false;
        var keyFormat:TextFormat=keyText.getTextFormat(); keyFormat.align="center"; keyFormat.leading=0;
        keyText.setTextFormat(keyFormat); keyText.setNewTextFormat(keyFormat);
        keyText._height=keyText.textHeight+4;
        keyText._y=(23-keyText._height)/2;
    }

    static function badge(parent:MovieClip,name:String,key:String,x:Number,y:Number,font:String,ring:Number,ink:Number):Number {
        var mc:MovieClip=parent.createEmptyMovieClip(name,parent.getNextHighestDepth());
        mc._x=x; mc._y=y;
        if(key.indexOf("/")>=0) {
            var keys:Array=key.split("/");
            var first:Number=badge(mc,"first",keys[0],0,0,font,ring,ink);
            badge(mc,"second",keys[1],first+6,0,font,ring,ink);
            return badgeWidth(key);
        }
        var w:Number=capWidth(key);
        if(isFace(key)) {
            mc.lineStyle(1,0xFFFFFF,70);
            mc.beginFill(0x000000,85); circle(mc,12,12,11); mc.endFill();
            if(!playstation || key=="A" || key=="B" || key=="Y") { keyLabel(mc,key,24,14,0xFFFFFF,font); return 24; }
            mc.lineStyle(1.6,0xFFFFFF,95);
            if(key=="X") { mc.moveTo(8,8); mc.lineTo(16,16); mc.moveTo(16,8); mc.lineTo(8,16); }
            else if(key=="O") circle(mc,12,12,5);
            else if(key=="SQR") { mc.moveTo(7.5,7.5); mc.lineTo(16.5,7.5); mc.lineTo(16.5,16.5); mc.lineTo(7.5,16.5); mc.lineTo(7.5,7.5); }
            else { mc.moveTo(12,6.5); mc.lineTo(17.5,16.5); mc.lineTo(6.5,16.5); mc.lineTo(12,6.5); }
            return 24;
        }
        if(isIcon(key)) {
            mc.lineStyle(1,0xFFFFFF,70);
            mc.beginFill(0x000000,85); circle(mc,12,12,11.5); mc.endFill();
            mc.lineStyle(1.4,0xFFFFFF,95);
            if(key=="Wheel") {
                mc.moveTo(12,5); mc.lineTo(12,19);
                mc.moveTo(9,8); mc.lineTo(12,5); mc.lineTo(15,8);
                mc.moveTo(9,16); mc.lineTo(12,19); mc.lineTo(15,16);
                mc.moveTo(9,10); mc.lineTo(15,10); mc.lineTo(15,14); mc.lineTo(9,14); mc.lineTo(9,10);
            } else {
                mc.moveTo(12,5); mc.lineTo(12,19); mc.moveTo(5,12); mc.lineTo(19,12);
                mc.moveTo(9,8); mc.lineTo(12,5); mc.lineTo(15,8);
                mc.moveTo(9,16); mc.lineTo(12,19); mc.lineTo(15,16);
                mc.moveTo(8,9); mc.lineTo(5,12); mc.lineTo(8,15);
                mc.moveTo(16,9); mc.lineTo(19,12); mc.lineTo(16,15);
            }
            return 24;
        }
        // Keyboard key or shoulder button: black box, thin white border, white text.
        mc.lineStyle(1,0xFFFFFF,70);
        mc.beginFill(0x000000,85); roundRect(mc,w,23,3); mc.endFill();
        if(key=="Enter") {
            mc.lineStyle(1.6,0xFFFFFF,95);
            mc.moveTo(17,6); mc.lineTo(17,12); mc.lineTo(7,12);
            mc.moveTo(10,9); mc.lineTo(7,12); mc.lineTo(10,15);
            return w;
        }
        keyLabel(mc,key,w,key.length>2?11:13,0xFFFFFF,font);
        return w;
    }
}
