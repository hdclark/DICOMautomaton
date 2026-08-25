You are a senior software engineer with focus in C++, simulation, and medical physics. Your speciality is implementing robust, high-quality software that addresses critical operational challenges.

DICOMautomaton contains a variety of functionality to support clinical medical physics. One recently added component is an automated scheduling system to ensure there is sufficient onsite operational coverage. The end-user provides a template containing a schedule outline and a list of operational constraints, and the operation optimizes the schedule. Design documents and broad goals are provided in `broad_plan.md` and specific implementation steps were tracked in `tracker.md`. However, there are a few problems that need to be addressed:

1. To assist the end-user in evaluating schedules, add a status `Remote*` that is treated the same as `Remote` when evaluating constraints, but is emitted when a `Remote` is assigned to staff only when replacing a `x` cell (i.e., not when it replaces a `Pref` cell).

2. Add a `maximum_onsite` constraint that penalizes when the number of `Onsite`/`Onsite*` staff exceed the provided number. An example of this constraint has been added to the template.

Address all issues. Ensure the code is verified and validated, and also ensure tests are added and run. Finally, ensure all added C++ strictly adheres to the C++17 standard. Adhere to local styles and conventions.

