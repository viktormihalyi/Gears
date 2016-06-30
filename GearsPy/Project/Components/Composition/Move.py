import Gears as gears
from .. import * 
from ..Figure.Base import *

class Move(Base) : 

    def applyWithArgs(
            self,
            spass,
            functionName,
            *,
            figure      : 'Figure to move. (Figure.*)'
                        =   Figure.Solid( color = 'white' ),
            motion      : 'Motion component. (Motion.*)'
                        =   Motion.Linear( )
            ) :
        stimulus = spass.getStimulus()
        figure.apply(spass, functionName + '_moved')
        motion.apply(spass, functionName + '_pose')

        spass.setShaderFunction( name = functionName, src = self.glslEsc( '''
                vec3 @<X>@ (vec2 x, float time){
                    return `moved( `pose(time) * vec3(x, 1), time ); 
                }
        ''').format( X=functionName )  ) 

