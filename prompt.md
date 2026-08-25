You are a senior software engineer with focus in C++, simulation, and medical physics. Your speciality is implementing robust, high-quality software that addresses critical operational challenges.

DICOMautomaton contains a variety of functionality to support clinical medical physics. One recently added component is an automated scheduling system to ensure there is sufficient onsite operational coverage. The end-user provides a template containing a schedule outline and a list of operational constraints, and the operation optimizes the schedule. Design documents and broad goals are provided in `broad_plan.md` and specific implementation steps were tracked in `tracker.md`. However, there are a few problems that need to be addressed:

1. When the schedule template contains a row with at least one cell containing `Holiday`, the day should be ignored and no optimization is needed. Simply pass the day through to the output as-is without warning. Ensure the unnecessary date parsing and handling code is removed.

2. Dates are parsed and converted to a timestamp, but there is no specific need to do this. Instead of parsing the date, simply use the contents of the left-most non-empty column in the schedule rows as-is for messages. It is acceptable to detect 'weeks' as a contiguous block of dates after the repeated header.

3. The `SDL_Viewer` operation is the primary way that end-users will view schedules. The `SDL_Viewer` operation supports keyword highlighting, but the list is hard-coded, and it would not be appropriate to add schedule-specific keywords enabled as default. Instead, allow the end-user to pass keywords and highlght colours in as an operation parameter to `SDL_Viewer`. Use the `parse_functions` routine from `src/String_Parsing.{h,cc}` to accept an arbitrary number of inputs, but provide the existing hardcoded defaults as default operation parameters.

4. The final output schedule has all staff as `Remote` on any possible day, despite all the myriad of violations. Assess why the final schedule is always this way and focus on more viable schedules.

Address all issues. Ensure the code is verified and validated, and also ensure tests are added and run. Finally, ensure all added C++ strictly adheres to the C++17 standard. Adhere to local styles and conventions.

