/* Fan speed controller
   Adjusts the fan speed according to cpu temperature.
*/
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>


// lowest pwm value to use (since the fan may not run with very low values)
#define MIN_PWM 50.0

// max pwm value to use (useful if fan is rated for lower voltage than the supply)
#define MAX_PWM 100.0

// pwm to use when first turning the fan on (the 'kick')
#define KICK_PWM 70.0

// how long the kick should last (microseconds)
#define KICK_TIME 500000

// lowest temperature to run the fan
#define LOW_TEMP 55

// temperature at which the fan should be full speed
#define HIGH_TEMP 75

// loop iteration time
#define LOOPTIME 2

// how long the fan should run after we drop below low temp
#define RUNONTIME 30
// convert time to loops for ease of use
#define RUNONLOOPS (RUNONTIME/LOOPTIME)

// Set this to not run as a daemon
//#define DEBUG

/* Get the cpu temperature
   Returns the temperature in degress C, or 0 if there was a problem
*/
float getTemp()
{
  FILE * tfile = fopen("/sys/class/hwmon/hwmon0/device/temp_label", "r");
  if (!tfile) {
    fprintf(stderr, "failed to open temperature file\n"); // TODO replace with log
    return 0;
  }

  int temp;
  int r = fscanf(tfile, "%d", &temp);

  fclose(tfile);

  if (r==1) {
    return (float)temp;
  } else {
    return 0;
  }

} // getTemp()

/* Set the fan speed
   @param % duty cycle
*/
void setFanSpeed(int duty)
{
  FILE * pfile = fopen("/sys/devices/platform/pwm/pwm.0", "w");
  if (!pfile) {
    fprintf(stderr, "failed to open pwm.0 for writing\n");
    return;
  }

  // Format is freq,duty
  // 10 kHz is audible. 50 kHz is silent and still has lots of
  // resolution for the duty cycle.
  fprintf(pfile,"50000,%d\n", duty);

  fclose(pfile);

}

void main()
{
  // check we have root access
  int euid = geteuid();
  if (euid!=0) {
    fprintf(stderr, "Must be run as root\n");
    exit(1);
  }

#ifndef DEBUG
  /* Our process ID and Session ID */
  pid_t pid, sid;
        
  /* Fork off the parent process */
  pid = fork();
  if (pid < 0) {
    exit(EXIT_FAILURE);
  }
  /* If we got a good PID, then
     we can exit the parent process. */
  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }

  /* Change the file mode mask */
  umask(0);
                
  /* Open any logs here */        
  FILE *flog = fopen("/var/log/pfan.log", "a");
  if (!flog) {
    fprintf(stderr, "failed to open log file\n");
    exit(EXIT_FAILURE);
  }

  fprintf(flog, "pfand started\n");
  fflush(flog);
                
  // check that we can read the temperature
  if (getTemp()==0.0) {
    fprintf(flog, "unable to read the temperature (or it's zero degrees?), quitting\n");
    exit(EXIT_FAILURE);
  }

  /* Create a new SID for the child process */
  sid = setsid();
  if (sid < 0) {
    /* Log the failure */
    exit(EXIT_FAILURE);
  }
        
  /* Change the current working directory */
  if ((chdir("/")) < 0) {
    /* Log the failure */
    exit(EXIT_FAILURE);
  }

  /* Close out the standard file descriptors */
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
#endif


  // Give fan an initial full speed run so that we can see it's working during start up
  setFanSpeed(MAX_PWM);

  // sleep 5s
  sleep(5);

  // counter for the fan run-on feature
  int runOn=0;

  int prevP=0;

  // control loop
  for(;;) { // loop forever

    // get the current cpu temperature
    float temp=getTemp(); 
    //printf("temp %f\n", temp);

    // calculate the corresponding pwm value
    int p;
    if (temp < LOW_TEMP) {
      if (runOn>0) {
        runOn--;
        p = MIN_PWM;
      } else {
        p = 0;
      }
    } else if (temp > HIGH_TEMP) {
      p = MAX_PWM;
    } else {
      // between MIN_PWM and MAX_PWM
      float dt = temp - LOW_TEMP;
      float pRange = MAX_PWM - MIN_PWM;
      float tRange = HIGH_TEMP-LOW_TEMP;

      p = floor(pRange * dt / tRange + MIN_PWM);

      // just in case I got the calculation wrong...
      if (p>MAX_PWM) {
        fprintf(stderr, "PWM calculation wrong, p is %d\n", p);
        p = MAX_PWM;
      }

      if (p<MIN_PWM) {
        fprintf(stderr, "PWM calculation wrong, p is %d\n", p);
        p = MIN_PWM;
      }
      // reset the counter
      runOn = RUNONLOOPS;
    }

#ifdef DEBUG
    printf("Setting pwm %d\n", p);
#endif

    // kick?
    if (p>0 && prevP==0) {
      setFanSpeed(KICK_PWM);
      usleep(KICK_TIME);
    }

    // set the pwm value
    setFanSpeed(p);

    prevP = p;

    sleep(LOOPTIME);

  } // control loop

  exit(EXIT_SUCCESS);

}
