import Foundation
import CoreGraphics
import ImageIO
let size = 1024
let space = CGColorSpace(name: CGColorSpace.sRGB)!
let c = CGContext(data:nil,width:size,height:size,bitsPerComponent:8,bytesPerRow:0,space:space,bitmapInfo:CGImageAlphaInfo.premultipliedLast.rawValue)!
func color(_ r:CGFloat,_ g:CGFloat,_ b:CGFloat,_ a:CGFloat=1)->CGColor {CGColor(colorSpace:space,components:[r,g,b,a])!}
let rect=CGRect(x:48,y:48,width:928,height:928)
c.addPath(CGPath(roundedRect:rect,cornerWidth:208,cornerHeight:208,transform:nil));c.clip()
let gradient=CGGradient(colorsSpace:space,colors:[color(0.16,0.07,0.29),color(0.32,0.13,0.45)] as CFArray,locations:[0,1])!
c.drawLinearGradient(gradient,start:CGPoint(x:512,y:48),end:CGPoint(x:512,y:976),options:[])
let glow=CGGradient(colorsSpace:space,colors:[color(1,0.42,0.34,0.28),color(0.7,0.3,0.7,0)] as CFArray,locations:[0,1])!
c.drawRadialGradient(glow,startCenter:CGPoint(x:512,y:580),startRadius:0,endCenter:CGPoint(x:512,y:580),endRadius:400,options:[])
c.saveGState();c.addEllipse(in:CGRect(x:270,y:370,width:484,height:484));c.clip()
for y in stride(from:370,to:856,by:46) {
 c.setFillColor(color(1,CGFloat(y-370)/484*0.2+0.48,0.39));c.fill(CGRect(x:260,y:y,width:510,height:y<600 ? 29:42))
}
c.restoreGState()
c.setFillColor(color(0.18,0.09,0.31));c.beginPath();c.move(to:CGPoint(x:0,y:330));
for point in [CGPoint(x:190,y:453),CGPoint(x:332,y:347),CGPoint(x:434,y:414),CGPoint(x:602,y:288),CGPoint(x:776,y:446),CGPoint(x:1024,y:339),CGPoint(x:1024,y:0),CGPoint(x:0,y:0)] {c.addLine(to:point)}
c.closePath();c.fillPath()
c.setStrokeColor(color(0.71,0.5,0.96,0.48));c.setLineWidth(3)
for x in stride(from:-600,through:1600,by:200) {c.move(to:CGPoint(x:x,y:0));c.addLine(to:CGPoint(x:512,y:330))}
for y in [80,155,212,253,284,307,323] {c.move(to:CGPoint(x:48,y:y));c.addLine(to:CGPoint(x:976,y:y))};c.strokePath()
// A quiet waveform bridges the horizon and makes the music purpose visible.
c.setStrokeColor(color(1,0.74,0.49));c.setLineWidth(7);c.setLineCap(.round)
for i in 0..<17 {let x=336+i*22;let height=25+abs(sin(Double(i)*0.83))*56
 c.move(to:CGPoint(x:Double(x),y:204-height/2));c.addLine(to:CGPoint(x:Double(x),y:204+height/2))};c.strokePath()
let destination=CGImageDestinationCreateWithURL(URL(fileURLWithPath:"assets/sunroom_icon.png") as CFURL,"public.png" as CFString,1,nil)!
CGImageDestinationAddImage(destination,c.makeImage()!,nil);assert(CGImageDestinationFinalize(destination))
print("SUNROOM icon generated")
