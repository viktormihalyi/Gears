import Gears as gears
from .. import * 
from .Base import *

class MercatorOblique(Base) : 

    def applyWithArgs(
            self,
            patch,
            functionName,
            ) :

        experiment = patch.getExperiment()

        patch.setShaderFunction( name = functionName, src = self.glslEsc( '''
                vec2 @<X>@(vec2 g){
                    float x = g.x * 0.003;
                    float y = g.y * 0.006;
                    float la1 = 0.5;
                    float la2 = 1.5;
                    float phi1 = 1;
                    float phi2 = 2;
                    float lap = atan( 
                        (cos(phi1) * sin(phi1) * cos(la1) - sin(phi1) * cos(phi1) * cos(la1))
                        /
                        (sin(phi1) * cos(phi1) * sin(la2) - cos(phi1) * sin(phi1) * sin(la1))
                        );
                    float phip = atan( - cos(lap - la1) / tan(phi1));
                    
                    return vec2( 
                        1000 * asin( sin(phip) * tanh(y) + cos(phip) * sin(x) / cosh(y) ),
                        1000 * atan( (sin(phip) * sin(x) - cos(phip) * sinh(y)) / cos(x) )
                        );
                    }
        ''').format( X=functionName )  ) 
