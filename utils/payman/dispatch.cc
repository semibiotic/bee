// File Shell
// Dispatch file

#include "global.h"

extern int EventType;

//       Dispatch queue
// 1. Program specific keys / events
// 2. Current panel (panel internal hotkeys)
// 3. Command line keys

//       Returns (cycle verdict)
// RET_CONT - continue cycle
// RET_EXIT - Program exit selected
// RET_EXEC - Execute shell command (Pass command to OSshel)
// RET_TERM - Terminal resized - rearrange Areas
//   (Area procedures return values, exept RET_DONE)
// RET_DONE - Event dispatched (by Area proc), interrupt queue

int	DispEvent()
{
   static int fEnabled;
   static int fVisible=1;
   static int nLmode=1;
   static int nRmode=1;
   static int nChoice=0;
//-------------------------- Terminal ----------------------------------------
   if (keyLong==K_TERMRESIZE) 
   {  GetSizes();
      return RET_TERM;
   }
//--------------------------- Program keys ------------------------------------
   switch(keyLong & M_KEY)
   {  case K_ENTER:
         return RET_EXEC;
      case K_F(10):
         if (MessageBox("Quit program\0",
	                "  Do you really want to"
                        " \nquit Closed Commander\0",
	                MB_YESNO|MB_NEUTRAL)==ID_YES) return RET_EXIT;

         return RET_DONE;
      case K_ESC: 
         return RET_EXIT;
      case K_F(8):
         testDialog.Dialog(0);
	 return RET_DONE;
      case K_F(9):
        {  int rets;
           OptionsMenuControls[0].val=fEnabled?0:1;
           if (fVisible)  
           {  OptionsMenuControls[1].val=0;
	      LeftMenuControls[11].style&=(~CS_HIDDEN);
	      LeftMenuControls[12].style&=(~CS_HIDDEN);
	      RightMenuControls[11].style&=(~CS_HIDDEN);
	      RightMenuControls[12].style&=(~CS_HIDDEN);
	      if (fEnabled)
	      {  LeftMenuControls[11].style&=(~CS_DISABLED);
	         LeftMenuControls[12].style&=(~CS_DISABLED);
	         RightMenuControls[11].style&=(~CS_DISABLED);
	         RightMenuControls[12].style&=(~CS_DISABLED);
	      } else
	      {  LeftMenuControls[11].style|=CS_DISABLED;
	         LeftMenuControls[12].style|=CS_DISABLED;
	         RightMenuControls[11].style|=CS_DISABLED;
	         RightMenuControls[12].style|=CS_DISABLED;
	      } 
	   } else
           {  OptionsMenuControls[1].val=1;
	      LeftMenuControls[11].style|=CS_HIDDEN|CS_DISABLED;
	      LeftMenuControls[12].style|=CS_HIDDEN|CS_DISABLED;
	      RightMenuControls[11].style|=CS_HIDDEN|CS_DISABLED;
	      RightMenuControls[12].style|=CS_HIDDEN|CS_DISABLED;
	   }
           for (int i=0;i<5;i++)
	   {  RightMenuControls[i].val= i==nRmode?1:0;
	      LeftMenuControls[i].val= i==nLmode?1:0;
	      if (i<3) FileMenuControls[i].val= i==nChoice?1:0;
	   }
	   rets=PullDn.Dialog(0);
           if (!rets) return RET_DONE;
	   if (!(rets & K_MENUID))
	   {  
	      keyLong=rets;
	      return RET_REDO;
	   }
           if (rets<MI_CH1 && (rets&0x0f)<6) 
	   {  *((rets&0x10)?&nRmode:&nLmode)=rets&0x0f-1;
	      return RET_DONE;
	   }
	   if (rets>=MI_CH1 && (rets&0x0f)<3)
	   {  nChoice=rets&0x0f;
	   }
           if (rets==MI_MOUNTDISABLE) fEnabled=!fEnabled;
           if (rets==MI_MOUNTHIDE) fVisible=!fVisible;
           return RET_DONE;   
        }
   } // (switch)
//------------------------- Command line dispatch -----------------------------
  (Area[AI_CMDVIEW].proc)(&Area[AI_CMDVIEW],OA_DISPEVENT,0);

  return RET_DONE;
}